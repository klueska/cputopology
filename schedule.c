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

/* An array containing the number of nodes at each level. */
static int num_nodes[NUM_NODE_TYPES];
static int **score;

/* An array containing the number of children at each level. */
static int num_children[NUM_NODE_TYPES];

/* An array containing the max refcounts of nodes at each level. */
static int max_refcount[NUM_NODE_TYPES][NUM_NODE_TYPES];

/* A list of lookup tables to find specific nodes by type and id. */
static int total_nodes;
static struct node *node_list;
static struct node *node_lookup[NUM_NODE_TYPES];

/* Forward declare some functions. */
static struct node *alloc_node(struct node *n);
static struct node *alloc_node_specific(int id, int type);
static struct node *alloc_node_any(int type);

/* Create a node and initialize it. */
static void init_nodes(int type, int num, int nchildren)
{
    /* Initialize the lookup tables for this node type. */
    num_nodes[type] = num;
    num_children[type] = nchildren;
    max_refcount[type][type] = 1;
    node_lookup[type] = node_list;
	
    for (int i = 0; i < type; i++) {
	max_refcount[type][i] = 1;
	for (int j = 0; j <= i; j++) {
	    max_refcount[type][j] *= num_children[i + 1];
	}
	node_lookup[type] += num_nodes[i];
    }

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

/* Allocate a flat array of scores. */
static void init_score()
{
    if ((score = malloc(num_cores * sizeof(int*))) != NULL)
	for (int i = 0; i < num_cores; i++ ) {
	    if ((score[i] = malloc(num_cores*sizeof(int))) == NULL )
		exit(-1);
	}
    for (int i = 0; i < num_cores; i++ ) {
	for (int j = 0; j < num_cores; j++ ) {
	    if (i == j) {
		score[i][j] = 0;
	    } else {
		int chip_id_i = i/cores_per_chip;
		int chip_id_j = j/cores_per_chip;
		if (chip_id_i == chip_id_j) {
		    score[i][j] = 2;
		} else {
		    int socket_id_i = i/cores_per_socket ;
		    int socket_id_j = j/cores_per_socket ;
		    if (socket_id_i == socket_id_j) {
			score[i][j] = 4;
		    } else {
			int numa_id_i = i/cores_per_numa ;
			int numa_id_j = j/cores_per_numa ;
			if (numa_id_i == numa_id_j) {
			    score[i][j] = 6;
			} else {
			    score[i][j] = 8;
			}
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
    total_nodes = num_cores + num_chips + num_sockets + num_numa;
    node_list = malloc(total_nodes * sizeof(struct node));

    init_score();

    /* Initialize the nodes at each level in our hierarchy. */
    init_nodes(CORE, num_cores, 0);
    init_nodes(CHIP, num_chips, cores_per_chip);
    init_nodes(SOCKET, num_sockets, chips_per_socket);
    init_nodes(NUMA, num_numa, sockets_per_numa);
}

/* Returns the first child of type in parameter for the node n. */
static struct node *first_node(struct node *n, int type)
{
    struct node *first_child = n;
    while (first_child->type != type) 
	first_child = &first_child->children[0];
    return first_child;
}

/* Returns the best node to allocate in case alloc_any_node() is called. */
static struct node *find_best_node(int type)
{
    int best_refcount = 0;
    struct node *bestn = NULL;
    struct node *n = NULL;
    struct node *siblings = node_lookup[NUMA];
    int num_siblings = 1;

    for (int i = NUMA; i >= type; i--) {
	for (int j = 0; j < num_siblings; j++) {
	    n = &siblings[j];
	    if (n->refcount[type] == best_refcount)
		return first_node(n, type);
	    if (n->refcount[type] >= best_refcount &&
		n->refcount[type] < max_refcount[i][type]) {
		best_refcount = n->refcount[type];
		bestn = n;
	    }
	}
	if (i == type || bestn == NULL)
	    break;
	siblings = bestn->children;
	num_siblings = num_children[i];
	best_refcount = 0;
	bestn = NULL;
    }
    return bestn;
}

/* Returns the score of one core from the cores owned by proc p */
static int calc_score(struct node *n, struct core_list cl)
{
    int ret_score = 0;
    struct node *temp = NULL;
    STAILQ_FOREACH(temp, &cl, link) {
	ret_score += score[n->id][temp->id];
    }
    return ret_score;
}

/* Consider first core siblings of the cores the proc already own. Calculate for
   every possible node its score (sum of distance from this core to the one the 
   proc owns. Allocate the core that has the lowest score. */
static struct node *find_best_core(struct proc *p)
{
    int best_score = 0;
    struct core_list core_owned = p->core_owned;
    struct node *bestn = NULL;
    struct node *np = NULL;
    int sibling_id = 0;
    STAILQ_FOREACH(np, &core_owned, link) {
	sibling_id = np->id % cores_per_chip == 0 ? np->id+1: np->id-1;
/* We first consider the core siblings */
	struct node *core_sibling = &node_lookup[CORE][sibling_id];
	if (core_sibling->refcount[CORE] == 0) {
	    int sibling_score = calc_score(core_sibling,core_owned) ;
	    if (best_score == 0 || sibling_score < best_score) {
		best_score = sibling_score ;
		bestn = core_sibling;
	    }
	}
/* We then consider the core of the chip siblings */
	if (STAILQ_NEXT(np,link) == NULL && bestn == NULL) {
	    int socket_id = np->id / cores_per_socket;
	    for (int i = 0; i<cores_per_socket; i++) {
		sibling_id = i + cores_per_socket*socket_id;
		struct node *core_sibling = &node_lookup[CORE][sibling_id];
		if (core_sibling->refcount[CORE] == 0) {
		    int sibling_score = calc_score(core_sibling,core_owned) ;
		    if (best_score == 0 || sibling_score < best_score) {
			best_score = sibling_score ;
			bestn = core_sibling;
		    }
		}
	    }
	}
    }
    return bestn;
}

/* Returns the best first node to allocate for a proc whic has no core. 
   Return the node that is the farthest from the others. */
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
		return first_node(n, CORE);
	    if (best_refcount == 0)
		best_refcount = n->refcount[CORE];
	    if (n->refcount[CORE] <= best_refcount &&
		n->refcount[CORE] < max_refcount[i][CORE]) {
		best_refcount = n->refcount[CORE];
		bestn = n;
	    }
	}
	if (i == CORE || bestn == NULL)
	    break;
	siblings = bestn->children;
	num_siblings = num_children[i];
	best_refcount = 0;
	bestn = NULL;
    }
    return bestn;
}

/* Recursively incref a node from its level through its ancestors.  At the
current level, we simply check if the refcount is 0, if it is not, we
increment it to one. Then, for each other lower level of the array, we sum
the refcount of the children. */
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
refcount is not 0, we have to check if the refcount of every child of the
current node is 0 to decrement its refcount. */
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

/* Allocate a specific node. */
static struct node *alloc_node(struct node *n)
{
    if (n == NULL)
	return NULL;
    if (n->refcount[n->type])
	return NULL;

    if (num_children[n->type] == 0)
	incref_node_recursive(n);
    for (int i = 0; i < num_children[n->type]; i++)
	alloc_node(&n->children[i]);
    return n;
}

/* Free a specific node. */
static int free_node(struct node *n)
{
    if (n == NULL)
	return -1;
    if (n->type != CORE)
	return -1;

    decref_node_recursive(n);
    return 0;
}

/* Allocates the *best* node from our node structure. *Best* could have
different interpretations, but currently it means to allocate nodes as
tightly packed as possible.  All ancestors of the chosen node will be
increfed in the process, effectively allocating them as well. */
static struct node *alloc_best_core(struct proc *p)
{
    return alloc_node(find_best_core(p));
}

static struct node *alloc_core_first()
{
    return alloc_node(find_first_core());
}

/* Allocates a specific node from our node structure. All ancestors will be
   increfed in the process, effectively allocating them as well. */
static struct node *alloc_core_id(int id)
{
    return alloc_node(&node_lookup[CORE][id]);
}

/* Frees a specific node back into our node structure. All ancestors will be
decrefed and freed as well if their refcounts hit 0. */
static int free_core_id(int id)
{
    return free_node(&node_lookup[CORE][id]);
}

/* This function turns a node into a list of its cores. For exemple, if the node
allocated is a socket of 8 cores, this function will return a list of the 8 
cores composing the socket. If we can't get one of the core of the node, we 
return an empty core list. */
static struct core_list node2list(struct node *n) 
{
    struct core_list core_available = 
	STAILQ_HEAD_INITIALIZER(core_available);
    if (n != NULL) {
	if (n->type == CORE) {
	    STAILQ_INSERT_TAIL(&core_available, 
			       &node_lookup[CORE][n->id], link);
	} else {
	    int cores_in_node = max_refcount[n->type][CORE];
	    for (int i = 0; i < cores_in_node; i++) {
		int index = i +	n->id*cores_in_node; 
		STAILQ_INSERT_TAIL(&core_available, 
				   &node_lookup[CORE][index],
				   link);
	    }
	}
    }
    return core_available;
}


/* Enable to handle multi "any" allocations given the number and the type of 
   nodes we want. Concat the list of cores and returns it. 
   If we can't get one of the requested core, we return an empty core list. */
static void concat_list(int amt, struct proc *p, enum node_type type) {
    if (amt > num_nodes[type])
	return;
    for (int i = 0; i < amt; i++) {
	if (STAILQ_FIRST(&(p->core_owned)) == NULL) {	
	    struct core_list temp = node2list(alloc_core_first());
	    p->core_owned = temp;
	} else {
	    struct core_list temp = node2list(alloc_best_core(p));
	    if (STAILQ_FIRST(&temp) == NULL)
		return;
	    else
		STAILQ_CONCAT(&(p->core_owned), &temp);
	}
    }
}

void alloc_core_any(int amt, struct proc *p)
{
    concat_list(amt, p, CORE);
}

void alloc_core_specific(int core_id, struct proc *p)
{  
    struct node* core_allocated = alloc_core_id(core_id);
    if (core_allocated != NULL)
	STAILQ_INSERT_TAIL(&(p->core_owned), 
			    core_allocated, link);
	
}

int free_core_specific(int core_id, struct proc *p)
{
    if (free_core_id(core_id) == 0)
	STAILQ_REMOVE(&(p->core_owned), 
		      &node_lookup[CORE][core_id], node, link);
}

void print_node(struct node *n)
{
    printf("%-6s id: %2d, type: %d, num_children: %2d",
	   node_label[n->type], n->id, n->type,
	   num_children[n->type]);
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

void test_structure(){
    struct node *np = NULL;
    struct proc *p1 = malloc(sizeof(struct proc));
    struct proc *p2 = malloc(sizeof(struct proc));
    struct core_list list = STAILQ_HEAD_INITIALIZER(list);	
    p1->core_owned = list;
    p2->core_owned = list;
    alloc_core_any(3,p1);
    alloc_core_specific(3,p2);
    alloc_core_any(1,p1);
    STAILQ_FOREACH(np, &(p1->core_owned), link) {
	printf("I am core %d, refcount: %d\n",
	       np->id,np->refcount[0]);
    }
    /* print_all_nodes(); */
    free_core_specific(4,p1);
    /* printf("\nAFTER\n\n"); */
    /* print_all_nodes(); */
    /* struct core_list test = alloc_core_any(1); */
    /* printf("\nAFTER\n\n"); */
    print_all_nodes();
}
