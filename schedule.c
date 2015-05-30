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

/* A list of our allocatable cores, chips, sockets and numa domains. */
CIRCLEQ_HEAD(node_list, node);
static struct node_list node_list[] = {
	CIRCLEQ_HEAD_INITIALIZER(node_list[CORE]),
	CIRCLEQ_HEAD_INITIALIZER(node_list[CHIP]),
	CIRCLEQ_HEAD_INITIALIZER(node_list[SOCKET]),
	CIRCLEQ_HEAD_INITIALIZER(node_list[NUMA])
};

/* A list of lookup tables to find specific nodes by type and id. */
static struct node **node_lookup[4];

/* Forward declare some functions. */
static struct node *request_node(struct node *n);
static struct node *request_node_specific(int id, int type);
static struct node *request_node_any(int type);

/* Create a node and initialize it. */
static void create_nodes(int type, int num, int num_children)
{
	/* Create the lookup table for this node type. */
	node_lookup[type] = malloc(num * sizeof(struct node*));

	for (int i = 0; i < num; i++) {
		/* Create and initialize all fields of each node. */
		struct node *n = malloc(sizeof(struct node));
		n->id = i;
		n->type = type;
		n->available = true;
		CIRCLEQ_INSERT_TAIL(&node_list[type], n, link);
		n->parent = NULL;
		n->children = malloc(num_children * sizeof(struct node*));
		for (int j = 0; j < num_children; j++) {
			/* We know that our child_type is always 1 less than our type. We
        	 * should eventually make this explicit in a table though. */
			n->children[j] = node_lookup[type - 1][j + i * num_children];
			n->children[j]->parent = n;
		}
		n->num_children = num_children;

		/* Fill in the lookup table. */
		node_lookup[type][i] = n;
	}
}

/* Build our available resources structure. Here we initialize the numas and
 * call init_sockets, init_chips and init_cores. */
void resources_init()
{
	/* Create all the nodes. */
	create_nodes(CORE, num_cores, 0);
	create_nodes(CHIP, num_chips, cores_per_chip);
	create_nodes(SOCKET, num_sockets, chips_per_socket);
	create_nodes(NUMA, num_numa, sockets_per_numa);
}

/* Remove a node from its list and make it unavailable. If it is already
 * unavailable, do nothing. */
static void remove_node(struct node *n)
{
	if (n->available) {
		CIRCLEQ_REMOVE(&node_list[n->type], n, link);
		n->available = false;
	}
}

/* This function removes the calling node from its list and recursively removes
 * its ancestors from their lists as well. */
static void remove_node_recursive(struct node *n)
{
	remove_node(n);
	if (n->parent != NULL)
		remove_node_recursive(n->parent);
}

/* Request a specific node by node type. */
static struct node *request_node(struct node *n)
{
	if (n == NULL)
		return NULL;
	if (!n->available)
		return NULL;

	for (int i = 0; i < n->num_children; i++)
		request_node(n->children[i]);
	remove_node_recursive(n);
	return n;
}

/* Request for any node of a given type. */
static struct node *request_node_any(int type)
{
	return request_node(CIRCLEQ_FIRST(&node_list[type]));
}

/* Request a specific node by id. */
static struct node *request_node_specific(int type, int id)
{
	return request_node(node_lookup[type][id]);
}

/* Returns the numa with the id in input if available, return NULL otherwise */
struct node *request_numa_any()
{
	return request_node_any(NUMA);
}

/* Returns the numa with the id in input if available, return NULL otherwise */
struct node *request_numa_specific(int numa_id)
{
	return request_node_specific(NUMA, numa_id);
}

/* Returns the first available socket, returns NULL if nothing is availbale */
struct node *request_socket_any()
{
	return request_node_any(SOCKET);
}
		
/* Returns the socket with the id in input if available, return NULL otherwise */
struct node *request_socket_specific(int socket_id)
{
	return request_node_specific(SOCKET, socket_id);
}

/* Returns the first available chip, returns NULL if nothing is availbale */
struct node *request_chip_any()
{
	return request_node_any(CHIP);
}

/* Returns the chip with the id in input if available, return NULL otherwise */
struct node *request_chip_specific(int chip_id)
{
	return request_node_specific(CHIP, chip_id);
}

/* Returns the first available core, returns NULL if nothing is availbale */
struct node *request_core_any()
{
	return request_node_any(CORE);
}

/* Returns the core with the id in input if available, return NULL otherwise */
struct node *request_core_specific(int core_id)
{
	return request_node_specific(CORE, core_id);
}

void print_available_resources()
{		
	struct node *np = NULL;
	CIRCLEQ_FOREACH(np, &node_list[CORE], link) {
		printf("core id: %d, type: %d, parent_id: %d, ",
		       np->id, np->type, np->parent->id);
		printf("parent_type: %d,next_core_available id: %d\n",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&node_list[CORE],
					 np, link)->id);
	}
	
	np = NULL;
	CIRCLEQ_FOREACH(np, &node_list[CHIP], link) {
		printf("chip id: %d, type: %d, parent_id: %d, ",
		       np->id, np->type, np->parent->id);
		printf("parent_type: %d,next_chip_available id: %d",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&node_list[CHIP],
					 np, link)->id);
		for (int i = 0; i< cores_per_chip; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
	
	np = NULL;
	CIRCLEQ_FOREACH(np, &node_list[SOCKET], link) {
		printf("socket id: %d, type: %d, parent_id: %d, ",
		       np->id, np->type, np->parent->id);
		printf("parent_type: %d,next_socket_available id: %d",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&node_list[SOCKET],
					 np, link)->id);
		for (int i = 0;  i< chips_per_socket; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
	
	np = NULL;
	CIRCLEQ_FOREACH(np, &node_list[NUMA], link) {
		printf("numa id: %d, type: %d, next_numa_available id: %d",
		       np->id, np->type,
		       CIRCLEQ_LOOP_NEXT(&node_list[NUMA],
					 np, link)->id);
		for (int i = 0; i< sockets_per_numa; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
}

// static void init_test(int nb_numas, int sockets_per_numa, enum node_type type
// 		      int chips_per_socket, int cores_per_chip)
// {
// 	int counter = 0;//cores_per_chip, chips_per_socket, sockets_per_numa
// 	int parent_id = 0;
// 	struct node *new_core = malloc(sizeof(struct node));
// 	new_core->id = i + chip_id * cores_per_chip;
// 	new_core->type = CORE;
// 	new_core->available = true;
	
// 	switch (type) {
// 	case CORE:
// 		core_lookup = (struct node **)malloc(
// 			num_cores*sizeof(struct node*));
// 		counter = cores_per_chip;
// 		parent_id = chip_id;
// 		break;
// 	case CHIP:
// 		lookup_array = chip_lookup;
// 		chip_lookup = (struct node **)malloc(
// 			num_chips*sizeof(struct node*));
// 		parent_id = socket_id;
// 		break;
// 	case SOCKET:
// 		socket_lookup = (struct node **)malloc(
// 			num_sockets*sizeof(struct node*));
// 		counter = sockets_per_numa;
// 		parent_id = numa_id;
// 		break;
// 	case NUMA:
// 		numa_lookup = (struct node **)malloc(
// 			num_numa*sizeof(struct node*));
// 		break;
// 	}
// 	return NULL;
// 	for (int i = 0; i< cores_per_chip; i++) {

// /\* Add our core to the list of available cores *\/
// 		CIRCLEQ_INSERT_TAIL(&core_list, new_core, link);
// /\* Link our cores to their chip parent *\/
// 		new_core->parent = find_chip(chip_id);
// /\* Link the parents (chips) of each cores *\/
// 		find_chip(chip_id)->children[i] = new_core;
// /\* Fil our socket lookup array *\/
// 		core_lookup[new_core->id] = new_core;
// 	}
// }
