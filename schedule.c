/*
 * Copyright (c) 2015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * Kevin Klues <klueska@cs.berkeley.edu>
 * 
 * The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/queue.h>
#include "schedule.h"
#include "topology.h" 

#define num_cores           (cpu_topology_info.num_cores)
#define num_cores_power2    (cpu_topology_info.num_cores_power2)
#define num_cpus            (cpu_topology_info.num_cpus)
#define num_sockets         (cpu_topology_info.num_sockets)
#define num_numa            (cpu_topology_info.num_numa)
#define cores_per_numa      (cpu_topology_info.cores_per_numa)
#define cores_per_socket    (cpu_topology_info.cores_per_socket)
#define cores_per_cpu       (cpu_topology_info.cores_per_cpu)
#define cpus_per_socket     (cpu_topology_info.cpus_per_socket)
#define cpus_per_numa       (cpu_topology_info.cpus_per_numa)
#define sockets_per_numa    (cpu_topology_info.sockets_per_numa)

#define child_node_type(t) ((t) - 1)
#define num_children(t) ((t) ? num_descendants[(t)][(t)-1] : 0)

/* An array containing the number of nodes at each level. */
static int num_nodes[NUM_NODE_TYPES];

/* A 2D array containing for all core i its distance from a core j. */
static int **core_distance;

/* An array containing the number of children at each level. */
static int num_descendants[NUM_NODE_TYPES][NUM_NODE_TYPES];

/* A list of lookup tables to find specific nodes by type and id. */
static int total_nodes;
static struct node *node_list;
static struct node *node_lookup[NUM_NODE_TYPES];

/* Forward declare some functions. */
static struct node *alloc_core(struct node *n, struct proc *p);

/* Create a node and initialize it. */
static void init_nodes(int type, int num, int nchildren)
{
	/* Initialize the lookup tables for this node type. */
	num_nodes[type] = num;
	node_lookup[type] = node_list;
	for (int i = CORE; i < type; i++)
		node_lookup[type] += num_nodes[i];

	/* Initialize all fields of each node. */
	for (int i = 0; i < num; i++) {
		struct node *n = &node_lookup[type][i];
		n->id = i;
		n->type = type;
		memset(n->refcount, 0, sizeof(n->refcount));
		n->parent = NULL;
		n->children = &node_lookup[child_node_type(type)][i * nchildren];
		for (int j = 0; j < nchildren; j++)
			n->children[j].parent = n;
	}
}

/* Allocate a flat array of core_distances. */
static void init_core_distances()
{
	if ((core_distance = calloc(num_cores, sizeof(int*))) != NULL) {
		for (int i = 0; i < num_cores; i++ ) {
			if ((core_distance[i] = calloc(num_cores,
										   sizeof(int))) == NULL)
				exit(-1);
		}
		for (int i = 0; i < num_cores; i++ ) {
			for (int j = 0; j < num_cores; j++ ) {
				for (int k = CPU; k<= NUMA; k++) {
					if (i/num_descendants[k][CORE] ==
					    j/num_descendants[k][CORE]) {
						core_distance[i][j] = k * 2;
						break;
					}
				}
			}
		}
	}
}

/* Build our available nodes structure. */
void nodes_init()
{
	/* Allocate a flat array of nodes. */
	total_nodes = num_cores + num_cpus + num_sockets + num_numa;
	node_list = malloc(total_nodes * sizeof(struct node));

	/* Initialize the number of descendants from our cpu_topology info. */
	num_descendants[CPU][CORE] = cores_per_cpu;
	num_descendants[SOCKET][CORE] = cores_per_socket;
	num_descendants[NUMA][CORE] = cores_per_numa;
	num_descendants[SOCKET][CPU] = cpus_per_socket;
	num_descendants[NUMA][CPU] = cpus_per_numa;
	num_descendants[NUMA][SOCKET] = sockets_per_numa;

	/* Initialize the nodes at each level in our hierarchy. */
	init_nodes(CORE, num_cores, 0);
	init_nodes(CPU, num_cpus, cores_per_cpu);
	init_nodes(SOCKET, num_sockets, cpus_per_socket);
	init_nodes(NUMA, num_numa, sockets_per_numa);

	/* Initialize our 2 dimensions array of core_distance */
	init_core_distances();
}

/* Returns the first child of type in parameter for the node n. */
static struct node *first_core(struct node *n)
{
	struct node *first_child = n;
	while (first_child->type != CORE)
		first_child = &first_child->children[0];
	return first_child;
}

/* Returns the core_distance of one core from the cores owned by proc p */
static int calc_core_distance(struct node *n, struct core_list cl)
{
	int d = 0;
	struct node *temp = NULL;
	STAILQ_FOREACH(temp, &cl, link) {
		d += core_distance[n->id][temp->id];
	}
	return d;
}

/* Consider first core siblings of the cores the proc already own. Calculate for
 * every possible node its core_distance (sum of distance from this core to the
 * one the proc owns. Allocate the core that has the lowest core_distance. */
static struct node *find_best_core(struct proc *p)
{
	int bestd = 0;
	struct core_list core_owned = p->core_owned;
	struct node *bestn = NULL;
	struct node *np = NULL;
	int sibling_id = 0;
	for (int k = CPU; k <= NUMA ; k++) {
		STAILQ_FOREACH(np, &core_owned, link) {
			int nb_cores = num_descendants[k][CORE];
			int type_id = np->id / nb_cores;
			for (int i = 0; i < nb_cores; i++) {
				sibling_id = i + nb_cores*type_id;
				struct node *core_sibling = &node_lookup[CORE][sibling_id];
				if (core_sibling->refcount[CORE] == 0) {
					int sibd = calc_core_distance(core_sibling, core_owned);
					if (bestd == 0 || sibd < bestd) {
						bestd = sibd;
						bestn = core_sibling;
					}
				}
			}
		}
		if (bestn != NULL)
			return bestn;
	}
	return NULL;
}

/* Returns the best first node to allocate for a proc whic has no core. 
 * Return the node that is the farthest from the others. */
static struct node *find_first_core()
{
	int best_refcount = 0;
	struct node *bestn = NULL;
	struct node *n = NULL;
	struct node *siblings = node_lookup[NUMA];
	int num_siblings = 1;

	for (int i = NUMA; i >= CORE; i--) {
		for (int j = 0; j < num_siblings; j++) {
			n = &siblings[j];
			if (n->refcount[CORE] == 0)
				return first_core(n);
			if (best_refcount == 0)
				best_refcount = n->refcount[CORE];
			if (n->refcount[CORE] <= best_refcount &&
			    n->refcount[CORE] < num_descendants[i][CORE]) {
				best_refcount = n->refcount[CORE];
				bestn = n;
			}
		}
		if (i == CORE || bestn == NULL)
			break;
		siblings = bestn->children;
		num_siblings = num_children(i);
		best_refcount = 0;
		bestn = NULL;
	}
	return bestn;
}

/* Recursively incref a node from its level through its ancestors.  At the
 * current level, we simply check if the refcount is 0, if it is not, we
 * increment it to one. Then, for each other lower level of the array, we sum
 * the refcount of the children. */
static void incref_node_recursive(struct node *n)
{
	int type;
	struct node *p;
	while (n != NULL) {
		type = n->type;
		if (n->refcount[type] == 0) {
			n->refcount[type]++;
			p = n->parent;
			while (p != NULL) {
				p->refcount[type]++;
				p = p->parent;
			}
		}
		n = n->parent;
	}
}

/* Recursively decref a node from its level through its ancestors.  If the
 * refcount is not 0, we have to check if the refcount of every child of the
 * current node is 0 to decrement its refcount. */
static void decref_node_recursive(struct node *n)
{
	int type;
	struct node *p;
	while (n != NULL) {
		type = n->type;
		if ((type == CORE) || (n->refcount[child_node_type(type)] == 0)) {
			n->refcount[type]--;
			p = n->parent;
			while (p != NULL) {
				p->refcount[type]--;
				p = p->parent;
			}
		}
		n = n->parent;
	}
}

/* Allocate a specific core. */
static struct node *alloc_core(struct node *n, struct proc *p)
{
	if (n == NULL || n->refcount[n->type] ||  n->allocated_to == p) {
		return NULL;
	} else {
		n->allocated_to = p;
		n->provisioned_to = NULL;
		if (num_children(n->type) == 0)
			incref_node_recursive(n);
		for (int i = 0; i < num_children(n->type); i++)
			alloc_core(&n->children[i], p);
		return n;
	}
}

/* Free a specific core. */
static int free_core(int core_id, struct proc *p)
{
	struct node *n = &node_lookup[CORE][core_id];
	if (n == NULL || n->type != CORE || n->allocated_to != p) {
		return -1;
	} else {
		n->allocated_to = NULL;
		STAILQ_REMOVE(&(p->core_owned), n, node, link);
		decref_node_recursive(n);
		return 0;
	}
}

/* Allocates the *best* node from our node structure. *Best* could have
 * different interpretations, but currently it means to allocate nodes as
 * tightly packed as possible.  All ancestors of the chosen node will be
 * increfed in the process, effectively allocating them as well. */
static struct node *alloc_best_core(struct proc *p)
{
	return alloc_core(find_best_core(p), p);
}

static struct node *alloc_first_core(struct proc *p)
{
	return alloc_core(find_first_core(), p);
}

/* This function turns a node into a list of its cores. For exemple, if the
 * node allocated is a socket of 8 cores, this function will return a list of
 * the 8 cores composing the socket. If we can't get one of the core of the
 * node, we return an empty core list. */
static struct core_list node2list(struct node *n) 
{
	struct core_list core_available = STAILQ_HEAD_INITIALIZER(core_available);
	if (n != NULL) {
		if (n->type == CORE) {
			STAILQ_INSERT_TAIL(&core_available,
							   &node_lookup[CORE][n->id], link);
		} else {
			int cores_in_node = num_descendants[n->type][CORE];
			for (int i = 0; i < cores_in_node; i++) {
				int index = i +	n->id*cores_in_node;
				STAILQ_INSERT_TAIL(&core_available,
								   &node_lookup[CORE][index], link);
			}
		}
	}
	return core_available;
}
 
/* Concat the core in parameter to the list of cores p owns. */
static void concat_list(struct node *n, struct proc *p) {
	struct core_list temp = node2list(n);
	if (STAILQ_FIRST(&(p->core_owned)) == NULL) {
		p->core_owned = temp;
	} else {
		if (STAILQ_FIRST(&temp) == NULL)
			return;
		else
			STAILQ_CONCAT(&(p->core_owned), &temp);
	}
}

void alloc_core_any(int amt, struct proc *p)
{
	for (int i = 0; i < amt; i++) {
		struct node* n = NULL;
		if (STAILQ_FIRST(&(p->core_owned)) == NULL)
			n = alloc_first_core(p);
		else
			n = alloc_best_core(p);
		concat_list(n, p);
	}
}

void alloc_core_specific(int core_id, struct proc *p)
{  
	struct node *n = &node_lookup[CORE][core_id];
	if (n->provisioned_to == p || (n->provisioned_to == NULL &&
								   n->allocated_to == NULL))
		concat_list(alloc_core(n, p), p);
}

void provision_core(int core_id, struct proc *p)
{
	struct node *n = &node_lookup[CORE][core_id];
	if (n->provisioned_to == NULL)
		n->provisioned_to = p;
}

void deprovision_core(int core_id, struct proc *p)
{
	struct node *n = &node_lookup[CORE][core_id];
	if (n->provisioned_to != NULL)
		n->provisioned_to = NULL;
}

void print_node(struct node *n)
{
	printf("%-6s id: %2d, type: %d, num_children: %2d",
	       node_label[n->type], n->id, n->type,
	       num_children(n->type));
	for (int i = n->type ; i>-1; i--) {
		printf(", refcount[%d]: %2d", i, n->refcount[i]);
	}
	if (n->parent) {
		printf(", parent_id: %2d, parent_type: %d\n",
		       n->parent->id, n->parent->type);
	} else {
		printf("\n");
	}
}

void print_nodes(int type)
{
	struct node *n = NULL;
	for (int i = 0; i < num_nodes[type]; i++) {
		print_node(&node_lookup[type][i]);
	}
}

void print_all_nodes()
{
	for (int i = NUMA; i >= CORE; i--)
		print_nodes(i);
}

void test_structure()
{
	struct node *np = NULL;
	struct proc *p1 = malloc(sizeof(struct proc));
	struct proc *p2 = malloc(sizeof(struct proc));
	struct core_list list = STAILQ_HEAD_INITIALIZER(list);

	p1->core_owned = list;
	p2->core_owned = list;
	alloc_core_any(3,p1);
	provision_core(3, p2);
	alloc_core_specific(3,p2);
	alloc_core_any(1,p1);
	//free_core(0,p2);
	STAILQ_FOREACH(np, &(p1->core_owned), link) {
		printf("I am core %d, refcount: %d\n", np->id,np->refcount[0]);
	}
	/* print_all_nodes(); */
	free_core(4,p1);
	/* printf("\nAFTER\n\n"); */
	/* print_all_nodes(); */
	/* struct core_list test = alloc_core_any(1); */
	/* printf("\nAFTER\n\n"); */
	print_all_nodes();
}
