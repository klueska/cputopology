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

/* Build our available nodes structure. */
void nodes_init()
{
	/* Allocate a flat array of nodes. */
	total_nodes = num_cores + num_chips + num_sockets + num_numa;
	node_list = malloc(total_nodes * sizeof(struct node));

	/* Initialize the nodes at each level in our hierarchy. */
	init_nodes(CORE, num_cores, 0);
	init_nodes(CHIP, num_chips, cores_per_chip);
	init_nodes(SOCKET, num_sockets, chips_per_socket);
	init_nodes(NUMA, num_numa, sockets_per_numa);
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

/* Recursively incref a node from its level through its ancestors. */
/* At the current level, we simply check if the refcount is 0, if it is */
/* not, we increment it to one. Then, for each other lower level  */
/* of the array, we sum the refcount of the children. */
static void incref_node_recursive(struct node *n)
{
	do {
		if (n->refcount[n->type] == 0)
			n->refcount[n->type]++;
		for (int i = 0; i < n->type; i++) {
			int sum_refcount = 0;
			for (int j = 0; j < num_children[n->type]; j++) {
				struct node child = n->children[j]; 
				sum_refcount += child.refcount[i];
			}
			n->refcount[i] = sum_refcount;
		}
		n = n->parent;
	} while (n != NULL);
}

/* Recursively decref a node from its level through its ancestors. */
/* If the refcount is not 0, we have to check if the refcount of every child */
/* of the current node is 0 to decrement its refcount. */
static void decref_node_recursive(struct node *n)
{
	do {
		int available_children = 0;
		for (int i = n->type; i >= 0; i--) {
			if (n->refcount[i] != 0) {
				for (int j = 0; j < num_children[i]; j++) {
					struct node child = n->children[j]; 
					if (child.refcount[child.type] == 0)
						available_children++;
				}
				if (available_children == num_children[i])
					n->refcount[i]--;
			}
		}
		n = n->parent;
	} while (n != NULL);
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
	if (n->type != CORE || n->refcount[n->type] == 0)
		return -1;

	decref_node_recursive(n);
	return 0;
}

/* Allocates the *best* node from our node structure. *Best* could have
 * different interpretations, but currently it means to allocate nodes as
 * tightly packed as possible.  All ancestors of the chosen node will be
 * increfed in the process, effectively allocating them as well. */
static struct node *alloc_node_any(int type)
{
	return alloc_node(find_best_node(type));
}

/* Allocates a specific node from our node structure. All ancestors will be
 * increfed in the process, effectively allocating them as well. */
static struct node *alloc_node_specific(int type, int id)
{
	return alloc_node(&node_lookup[type][id]);
}

/* Frees a specific node back into our node structure. All ancestors will be
 * decrefed and freed as well if their refcounts hit 0. */
static int free_node_specific(int type, int id)
{
	return free_node(&node_lookup[type][id]);
}

/* This function turns a node into a list of its cores. For exemple, if  */
/* the node allocated is a socket of 8 cores, this function will return a  */
/* list of the 8 cores composing the socket. */
/* If we can't get one of the core of the node, we return an empty core list. */
struct core_list node2list(struct node *n) 
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
nodes we want. Concat the list of cores and returns it. */
/* If we can't get one of the requested core, we return an empty core list. */
struct core_list concat_list(int amt, enum node_type type) {
	struct core_list list = STAILQ_HEAD_INITIALIZER(list);
	if (amt > num_nodes[type])
		return list;
	for (int i = 0; i < amt; i++) {
		struct core_list temp =	list;
			list = node2list(alloc_node_any(type));
		if (STAILQ_FIRST(&list) == NULL)
			return list;
		else
			STAILQ_CONCAT(&list, &temp);
	}
	return list;
}

struct core_list alloc_numa_any(int amt)
{
	return concat_list(amt, NUMA);
}

struct core_list alloc_numa_specific(int numa_id)
{
	return node2list(alloc_node_specific(NUMA, numa_id));
}

struct core_list alloc_socket_any(int amt)
{
	return concat_list(amt, SOCKET);
}
		
struct core_list alloc_socket_specific(int socket_id)
{
	return node2list(alloc_node_specific(SOCKET, socket_id));
}

struct core_list alloc_chip_any(int amt)
{
	return concat_list(amt,CHIP);
}

struct core_list alloc_chip_specific(int chip_id)
{
	return node2list(alloc_node_specific(CHIP, chip_id));
}

struct core_list alloc_core_any(int amt)
{
	return concat_list(amt,CORE);
}

struct core_list alloc_core_specific(int core_id)
{
	return node2list(alloc_node_specific(CORE, core_id));
}

int free_core_specific(int core_id)
{
	return free_node_specific(CORE, core_id);
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
	struct core_list test1 = alloc_chip_any(3);
	STAILQ_FOREACH(np, &test1, link) {
		printf("I am core %d, refcount: %d\n",
		       np->id,np->refcount[0]);
	}
	print_all_nodes();
}
