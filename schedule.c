/*
 * Copyright (c) 2015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
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

/* Create a node and initialize it. */
static void create_nodes(int type, int num, int child_type, int num_children)
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
			n->children[j] = node_lookup[child_type][j + i * num_children];
			n->children[j]->parent = n;
		}

		/* Fill in the lookup table. */
		node_lookup[type][i] = n;
	}
}

/* Build our available resources structure. Here we initialize the numas and
 * call init_sockets, init_chips and init_cores. */
void resources_init()
{
	/* Create all the nodes. */
	create_nodes(CORE, num_cores, -1, 0);
	create_nodes(CHIP, num_chips, CORE, cores_per_chip);
	create_nodes(SOCKET, num_sockets, CHIP, chips_per_socket);
	create_nodes(NUMA, num_numa, SOCKET, sockets_per_numa);
}

/* This function is always called by a core. It removes all of the calling
 * core's parents (chips, socket and num) from their available list. */
static void remove_parent(struct node *node)
{
	if (node->parent != NULL)
		remove_parent(node->parent);
	node->available = false;
	CIRCLEQ_REMOVE(&node_list[node->type], node, link);
}

/* Returns the first available numa, returns NULL if nothing is available */
struct node *request_numa_any()
{
	if (CIRCLEQ_EMPTY(&node_list[NUMA]))
		return NULL;
	struct node *first_numa = CIRCLEQ_FIRST(&node_list[NUMA]); 

	if (first_numa->available) {
		first_numa->available = false;
		for(int i = 0; i< sockets_per_numa; i++) {
			request_socket_specific(first_numa->children[i]->id);
		}
		CIRCLEQ_REMOVE(&node_list[NUMA], first_numa, link);
		return first_numa;
	} else {
		return NULL;
	}
}

/* Returns the numa with the id in input if available, return NULL otherwise */
struct node *request_numa_specific(int numa_id)
{
	struct node *first_numa = node_lookup[NUMA][numa_id];
	if (first_numa->available) {
		first_numa->available = false;
		for (int i = 0;  i< sockets_per_numa; i++) {
			request_socket_specific(first_numa->children[i]->id);
		}
		CIRCLEQ_REMOVE(&node_list[NUMA], first_numa, link);
		return first_numa;
	} else {
		return NULL;
	}
}

/* Returns the first available socket, returns NULL if nothing is availbale */
struct node *request_socket_any()
{
	if (CIRCLEQ_EMPTY(&node_list[SOCKET]))
		return NULL;
	struct node *first_socket = CIRCLEQ_FIRST(&node_list[SOCKET]); 
	if (first_socket->available) {
		first_socket->available = false;
		for (int i = 0;  i< chips_per_socket; i++) {
			request_chip_specific(first_socket->children[i]->id);
		}
		CIRCLEQ_REMOVE(&node_list[SOCKET], first_socket, link);
		return first_socket;
	} else {
		return NULL;
	}
}
		
/* Returns the socket with the id in input if available, return NULL otherwise */
struct node *request_socket_specific(int socket_id)
{
	struct node *first_socket = node_lookup[SOCKET][socket_id];
	if (first_socket->available) {
		first_socket->available = false;
		for (int i = 0; i< chips_per_socket; i++) {
			request_chip_specific(first_socket->children[i]->id);
		}
		CIRCLEQ_REMOVE(&node_list[SOCKET], first_socket, link);
		return first_socket;
	} else {
		return NULL;
	}
}

/* Returns the first available chip, returns NULL if nothing is availbale */
struct node *request_chip_any()
{
	if (CIRCLEQ_EMPTY(&node_list[CHIP]))
		return NULL;
	struct node *first_chip = CIRCLEQ_FIRST(&node_list[CHIP]); 
	if (first_chip->available) {
		first_chip->available = false;
		for (int i = 0; i< cores_per_chip; i++) {
			request_core_specific(first_chip->children[i]->id);
		}
		CIRCLEQ_REMOVE(&node_list[CHIP], first_chip, link);
		return first_chip;
	} else {
		return NULL;
	}
}

/* Returns the chip with the id in input if available, return NULL otherwise
 * call request_core(core_id) on child so we remove them from the available
 * list */
struct node *request_chip_specific(int chip_id)
{
	struct node *first_chip = node_lookup[CHIP][chip_id];
	if (first_chip->available) {
		first_chip->available = false;
		for (int i = 0; i< cores_per_chip; i++) {
			request_core_specific(first_chip->children[i]->id);
		}
		CIRCLEQ_REMOVE(&node_list[CHIP], first_chip, link);
		return first_chip;
	} else {
		return NULL;
	}
}

/* Returns the first available core, returns NULL if nothing is availbale */
struct node *request_core_any()
{
	if (CIRCLEQ_EMPTY(&node_list[CORE]))
		return NULL;
	struct node *first_core = CIRCLEQ_FIRST(&node_list[CORE]); 
	if (first_core->available) {
		first_core->available = false;
		CIRCLEQ_REMOVE(&node_list[CORE], first_core, link);
		remove_parent(first_core);
		return first_core;
	} else {
		return NULL;
	}
}

/* Returns the core with the id in input if available, return NULL otherwise */
struct node *request_core_specific(int core_id)
{
	struct node *first_core = node_lookup[CORE][core_id];
	if (first_core->available) {
		first_core->available = false;
		CIRCLEQ_REMOVE(&node_list[CORE], first_core, link);
		remove_parent(first_core);
		return first_core;
	} else {
		return NULL;
	}
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
