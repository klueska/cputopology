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
static struct sched_pnode *node_list;
static struct sched_pcore *core_list;
static struct sched_pnode *node_lookup[NUM_NODE_TYPES];

/* Forward declare some functions. */
static struct sched_pcore *alloc_core(struct proc *p, struct sched_pcore *c);

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
		struct sched_pnode *n = &node_lookup[type][i];
		n->id = i;
		n->type = type;
		memset(n->refcount, 0, sizeof(n->refcount));
		n->parent = NULL;
		n->children = &node_lookup[child_node_type(type)][i * nchildren];
		for (int j = 0; j < nchildren; j++)
			n->children[j].parent = n;

		n->spc_data = NULL;
		if (n->type == CORE) {
			n->spc_data = &core_list[n->id];
			n->spc_data->spn = n;
			n->spc_data->spc_info = &cpu_topology_info.core_list[n->id];
			n->spc_data->alloc_proc = NULL;
			n->spc_data->prov_proc = NULL;
		}
	}
}

/* Allocate a flat array of array of int. It represent the distance from one
 * core to an other. If cores are on the same CPU, their distance is 2, if they
 * are on the same socket, their distance is 4, on the same numa their distance
 * is 6. Otherwise their distance is 8.*/
static void init_core_distances()
{
	core_distance = calloc(num_cores, sizeof(int*));
	if (core_distance == NULL)
		exit(-1);
	for (int i = 0; i < num_cores; i++) {
		core_distance[i] = calloc(num_cores, sizeof(int));
		if (core_distance[i] == NULL)
			exit(-1);
	}
	for (int i = 0; i < num_cores; i++) {
		for (int j = 0; j < num_cores; j++) {
			for (int k = CPU; k<= MACHINE; k++) {
				if (i/num_descendants[k][CORE] ==
					j/num_descendants[k][CORE]) {
					core_distance[i][j] = k;
					break;
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
	void *nodes_and_cores = malloc(total_nodes * sizeof(struct sched_pnode) +
	                               num_cores * sizeof(struct sched_pcore));
	node_list = nodes_and_cores;
	core_list = nodes_and_cores + total_nodes * sizeof(struct sched_pnode);

	/* Initialize the number of descendants from our cpu_topology info. */
	num_descendants[CPU][CORE] = cores_per_cpu;
	num_descendants[SOCKET][CORE] = cores_per_socket;
	num_descendants[SOCKET][CPU] = cpus_per_socket;
	num_descendants[NUMA][CORE] = cores_per_numa;
	num_descendants[NUMA][CPU] = cpus_per_numa;
	num_descendants[NUMA][SOCKET] = sockets_per_numa;
	num_descendants[MACHINE][CORE] = num_cores;
	num_descendants[MACHINE][CPU] = num_cpus;
	num_descendants[MACHINE][SOCKET] = num_sockets;
	num_descendants[MACHINE][NUMA] = num_numa;

	/* Initialize the nodes at each level in our hierarchy. */
	init_nodes(CORE, num_cores, 0);
	init_nodes(CPU, num_cpus, cores_per_cpu);
	init_nodes(SOCKET, num_sockets, cpus_per_socket);
	init_nodes(NUMA, num_numa, sockets_per_numa);

	/* Initialize our 2 dimensions array of core_distance */
	init_core_distances();
}

/* Returns the first core for the node n. */
static struct sched_pcore *first_core(struct sched_pnode *n)
{
	struct sched_pnode *first_child = n;
	while (first_child->type != CORE)
		first_child = &first_child->children[0];
	return first_child->spc_data;
}

/* Returns the core_distance of one core from the list of cores in parameter */
static int calc_core_distance(struct sched_pcore_tailq cl,
							  struct sched_pcore *c)
{
	int d = 0;
	struct sched_pcore *temp = NULL;
	STAILQ_FOREACH(temp, &cl, alloc_next) {
		d += core_distance[c->spc_info->core_id][temp->spc_info->core_id];
	}
	return d;
}

/* Return the best core among the list of provisioned cores. This function is
 * slightly different from find_best_core in the way we just need to check the
 * cores itself, and don't need to check other levels of the topology. If no
 * cores are available we return NULL.*/
static struct sched_pcore *find_best_core_provision(struct proc *p)
{
	int bestd = 0;
	struct sched_pcore_tailq core_prov_available = p->ksched_data.
												   prov_not_alloc_me;
	struct sched_pcore_tailq core_alloc = p->ksched_data.alloc_me;
	struct sched_pcore *bestc = NULL;
	struct sched_pcore *c = NULL;
	STAILQ_FOREACH(c, &core_prov_available, prov_next) {
		int sibd = calc_core_distance(core_alloc, c);
		if (bestd == 0 || sibd < bestd) {
			bestd = sibd;
			bestc = c;
		}
	}
	return bestc;
}

#define get_node_id(core_info, level) \
	((level) == CPU     ? (core_info)->cpu_id : \
	 (level) == SOCKET  ? (core_info)->socket_id : \
	 (level) == NUMA    ? (core_info)->numa_id : \
	 (level) == MACHINE ? 1 : 0)

/* Consider first core provisioned proc by calling find_best_core_provision.
 * Then check siblings of the cores the proc already own. Calculate for
 * every possible node its core_distance (sum of distance from this core to the
 * one the proc owns. Allocate the core that has the lowest core_distance. */
static struct sched_pcore *find_best_core(struct proc *p)
{
	struct sched_pcore *bestc = find_best_core_provision(p);

	/* If we found an available provisioned core, return it. */
	if (bestc != NULL)
		return bestc;

	/* Otherwise, keep looking... */
	int bestd = 0;
	struct sched_pcore *c = NULL;
	int sibling_id = 0;
	struct sched_pcore_tailq core_owned = p->ksched_data.alloc_me;

	for (int k = CPU; k <= MACHINE; k++) {
		STAILQ_FOREACH(c, &core_owned, alloc_next) {
			int nb_cores = num_descendants[k][CORE];
			int type_id = get_node_id(c->spc_info, k);
			for (int i = 0; i < nb_cores; i++) {
				sibling_id = i + nb_cores*type_id;
				struct sched_pcore *sibc = &core_list[sibling_id];
				if (sibc->alloc_proc == NULL) {
					int sibd = calc_core_distance(core_owned, sibc);
					if (bestd == 0 || sibd <= bestd) {
						/* If the core we have found has best core is
						 * provisioned by an other proc, we try to find an
						 * equivalent core (in terms of distance) and allocate
						 * this core instead. */
						if (sibd == bestd) {
							if (bestc->prov_proc != NULL &&
								sibc->prov_proc == NULL) {
								bestd = sibd;
								bestc = sibc;
							}
						} else {
							bestd = sibd;
							bestc = sibc;
						}
					}
				}
			}
		}
		if (bestc != NULL)
			return bestc;
	}
	return NULL;
}

/* Returns the first provision core available. If none is found, return NULL */
static struct sched_pcore *find_first_provision_core(struct proc *p)
{
	return STAILQ_FIRST(&(p->ksched_data.prov_not_alloc_me));
}

/* Returns the best first core to allocate for a proc which owns no core.
 * Return the core that is the farthest from the others's proc cores. */
static struct sched_pcore *find_first_core(struct proc *p)
{
	struct sched_pnode *n = NULL;
	struct sched_pnode *bestn = NULL;
	int best_refcount = 0;
	struct sched_pnode *siblings = node_lookup[MACHINE];
	int num_siblings = 0;

	struct sched_pcore *c = find_first_provision_core(p);
	if (c != NULL)
		return c;

	for (int i = MACHINE; i >= CORE; i--) {
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
	return bestn->spc_data;
}

/* Recursively incref a node from its level through its ancestors.  At the
 * current level, we simply check if the refcount is 0, if it is not, we
 * increment it to one. Then, for each other lower level of the array, we sum
 * the refcount of the children. */
static void incref_nodes(struct sched_pnode *n)
{
	int type;
	struct sched_pnode *p;
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
static void decref_nodes(struct sched_pnode *n)
{
	int type;
	struct sched_pnode *p;
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

/* Allocate a specific core if it is available. In this case, we need to check
 * if the core n is provisioned by p but allocated to an other proc. Then we
 * have to allocate a new core to this other proc. Also, it is important here
 * to maintain our list of provision and allocated or not allocated cores.
 * TODO ? : We also have to check if the core n is provisioned by an other proc.
 * In this case, we should try to reprovision an other core to this proc. */
static struct sched_pcore *alloc_core(struct proc *p, struct sched_pcore *c)
{
	struct proc *owner = c->alloc_proc;

	if (c == NULL || owner == p)
		return NULL;

	incref_nodes(c->spn);
	if (c->prov_proc == p) {
		STAILQ_REMOVE(&(p->ksched_data.prov_not_alloc_me), c, sched_pcore, prov_next);
		STAILQ_INSERT_HEAD(&(p->ksched_data.prov_alloc_me), c, prov_next);
		if (owner != NULL) {
			/* TODO: Trigger something here to actually do the allocation in
			 * the kernel */
			STAILQ_REMOVE(&(owner->ksched_data.alloc_me), c, sched_pcore, alloc_next);
		}
	}
	c->alloc_proc = p;
	return c;
}

/* Free a specific core. */
static int free_core(struct proc *p, int core_id)
{
	struct sched_pcore *c = &core_list[core_id];

	if (c == NULL || c->alloc_proc != p || core_id > num_cores)
		return -1;

	c->alloc_proc = NULL;
	STAILQ_REMOVE(&(p->ksched_data.alloc_me), c, sched_pcore, alloc_next);
	if (c->prov_proc == p){
		STAILQ_REMOVE(&(p->ksched_data.prov_alloc_me),
					  c, sched_pcore, prov_next);
		STAILQ_INSERT_HEAD(&(p->ksched_data.prov_not_alloc_me), c, prov_next);
	}
	decref_nodes(c->spn);
	return 0;
}

/* Allocates the *best* node from our node structure. *Best* could have
 * different interpretations, but currently it means to allocate nodes as
 * tightly packed as possible.  All ancestors of the chosen node will be
 * increfed in the process, effectively allocating them as well. */
static struct sched_pcore *alloc_best_core(struct proc *p)
{
	return alloc_core(p, find_best_core(p));
}

static struct sched_pcore *alloc_first_core(struct proc *p)
{
	return alloc_core(p, find_first_core(p));
}

/* Allocate an amount of cores for proc p. Those cores are elected according to
 * the algorithm in find_best_core. */
void alloc_core_any(struct proc *p, int amt)
{
	if (amt <= num_cores) {
		for (int i = 0; i < amt; i++) {
			struct sched_pcore *c = NULL;
			if (STAILQ_FIRST(&(p->ksched_data.alloc_me)) == NULL)
				c = alloc_first_core(p);
			else
				c = alloc_best_core(p);
			STAILQ_INSERT_TAIL(&p->ksched_data.alloc_me, c, alloc_next);
		}
	}
}

int free_core_specific(struct proc* p, int core_id)
{
	return free_core(p, core_id);
}

/* Allocate a specific core to the proc p. */
void alloc_core_specific(struct proc *p, int core_id)
{
	if (core_id <= num_cores) {
		struct sched_pcore *c = &core_list[core_id];
		if (c->prov_proc == p) {
			struct sched_pcore *c = alloc_core(p, c);
			STAILQ_INSERT_TAIL(&p->ksched_data.alloc_me, c, alloc_next);
		}
	}
}

/* Remove the provision made by a proc for a core. */
static void deprovision_core(struct sched_pcore *c)
{
	struct proc *p = c-> prov_proc;
	c->prov_proc = NULL;
	if (c->alloc_proc == p)
		STAILQ_REMOVE(&(p->ksched_data.prov_alloc_me), c, sched_pcore, prov_next);
	else
		STAILQ_REMOVE(&(p->ksched_data.prov_alloc_me),
					  c, sched_pcore, prov_next);
}

/* Provision a given core to the proc p. */
void provision_core(struct proc *p, int core_id)
{
	if (core_id <= num_cores) {
		struct sched_pcore *c = &core_list[core_id];
		if (c->prov_proc != NULL)
			deprovision_core(c);
		c->prov_proc = p;
		if (c->alloc_proc == p)
			STAILQ_INSERT_TAIL(&p->ksched_data.prov_alloc_me, c, prov_next);
		else
			STAILQ_INSERT_TAIL(&p->ksched_data.prov_not_alloc_me, c, prov_next);
	}
}

void print_node(struct sched_pnode *n)
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
	struct sched_pnode *n = NULL;
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
	struct sched_pcore *c = NULL;
	struct proc *p1 = malloc(sizeof(struct proc));
	struct proc *p2 = malloc(sizeof(struct proc));
	STAILQ_INIT(&p1->ksched_data.alloc_me);
	STAILQ_INIT(&p2->ksched_data.alloc_me);
	STAILQ_INIT(&p1->ksched_data.prov_alloc_me);
	STAILQ_INIT(&p2->ksched_data.prov_alloc_me);
	STAILQ_INIT(&p1->ksched_data.prov_not_alloc_me);
	STAILQ_INIT(&p2->ksched_data.prov_not_alloc_me);

	provision_core(p1, 7);
	alloc_core_any(p1, 3);
	provision_core(p2, 3);
	alloc_core_specific(p2, 3);
	provision_core(p2, 4);
	alloc_core_any(p2, 1);
	provision_core(p1, 4);
	alloc_core_any(p1, 1);
	alloc_core_any(p1, 1);
	free_core_specific(p1, 7);
	provision_core(p2, 5);
	printf("Cores allocated:\n");
	STAILQ_FOREACH(c, &(p1->ksched_data.alloc_me), alloc_next) {
		printf("proc%d :core %d\n",1, c->spc_info->core_id);
	}
	printf("\n");
	STAILQ_FOREACH(c, &(p2->ksched_data.alloc_me), alloc_next) {
		printf("proc%d :core %d\n",2, c->spc_info->core_id);
	}
	printf("\nCores prov_allocated:\n");
	STAILQ_FOREACH(c, &(p1->ksched_data.prov_alloc_me), prov_next) {
		printf("proc%d :core %d\n",1, c->spc_info->core_id);
	}
	printf("\n");
	STAILQ_FOREACH(c, &(p2->ksched_data.prov_alloc_me), prov_next) {
		printf("proc%d :core %d\n",2, c->spc_info->core_id);
	}
	printf("\nCores prov_not_allocated:\n");
	STAILQ_FOREACH(c, &(p1->ksched_data.prov_not_alloc_me), prov_next) {
		printf("proc%d :core %d\n",1, c->spc_info->core_id);
	}
	printf("\n");
	STAILQ_FOREACH(c, &(p2->ksched_data.prov_not_alloc_me), prov_next) {
		printf("proc%d :core %d\n",2, c->spc_info->core_id);
	}
	printf("\n");
	print_all_nodes();
}
