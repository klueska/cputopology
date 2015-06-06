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

#include "schedule.h"

/* An array containing the number of nodes at each level. */
static int num_nodes[NUM_NODE_TYPES];

/* An array containing the number of children at each level. */
static int num_children[NUM_NODE_TYPES];

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
	int node_offset = 0;
	for (int i = 0; i < type; i++)
		node_offset += num_nodes[i];
	node_lookup[type] = &node_list[node_offset];
	num_nodes[type] = num;
	num_children[type] = nchildren;

	/* Initialize all fields of each node. */
	for (int i = 0; i < num; i++) {
		struct node *n = &node_lookup[type][i];
		n->id = i;
		n->type = type;
		n->refcount = 0;
		n->score = 0;
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

/* Update the score of every node in the topology. */
static void update_scores()
{
	struct node *n = NULL;
	for (int i = total_nodes - 1; i >= 0; i--) {
		n = &node_list[i];
		if (n->parent == NULL)
			n->score = n->refcount;
		else
			n->score = n->refcount + n->parent->score;
	}
}

/* Returns the best node to allocate in case alloc_any_node() is called. */
static struct node *find_best_node(int type) {
	struct node *n = NULL;
	struct node *bestn = NULL;
	for (int i = 0; i < num_nodes[type]; i++) {
		n = &node_lookup[type][i];
		if (n->refcount == 0) {
			if (bestn == NULL || n->score > bestn->score)
				bestn = n;
		}
	}
	return bestn;
}

/* Recursively incref a node from its level through its ancestors. */
static void incref_node_recursive(struct node *n)
{
	do {
		n->refcount++;
		n = n->parent;
	} while (n != NULL);
	update_scores();
}

/* Recursively decref a node from its level through its ancestors. */
static void decref_node_recursive(struct node *n)
{
	do {
		n->refcount--;
		n = n->parent;
	} while (n != NULL);
}

/* Allocate a specific node. */
static struct node *alloc_node(struct node *n)
{
	if (n == NULL)
		return NULL;
	if (n->refcount)
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
	if (n->refcount != 1)
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

struct node *alloc_numa_any()
{
	return alloc_node_any(NUMA);
}

struct node *alloc_numa_specific(int numa_id)
{
	return alloc_node_specific(NUMA, numa_id);
}

struct node *alloc_socket_any()
{
	return alloc_node_any(SOCKET);
}
		
struct node *alloc_socket_specific(int socket_id)
{
	return alloc_node_specific(SOCKET, socket_id);
}

struct node *alloc_chip_any()
{
	return alloc_node_any(CHIP);
}

struct node *alloc_chip_specific(int chip_id)
{
	return alloc_node_specific(CHIP, chip_id);
}

struct node *alloc_core_any()
{
	return alloc_node_any(CORE);
}

struct node *alloc_core_specific(int core_id)
{
	return alloc_node_specific(CORE, core_id);
}

int free_core_specific(int core_id)
{
	return free_node_specific(CORE, core_id);
}

void print_node(struct node *n)
{
	printf("%s id: %d, type: %d, refcount: %d, score %d, num_children: %d",
	       node_label[n->type], n->id, n->type,
	       n->refcount, n->score, num_children[n->type]);
	if (n->parent) {
		printf(", parent_id: %d, parent_type: %d\n",
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
	alloc_chip_specific(0);
	alloc_chip_specific(2);
	alloc_core_specific(7);
	alloc_core_any();
	print_all_nodes();
}
