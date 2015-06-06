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
/* An array containing the number of nodes at each level. */
static int num_nodes[NUM_NODE_TYPES];

/* A list of lookup tables to find specific nodes by type and id. */
static struct node *node_lookup[NUM_NODE_TYPES];

/* Forward declare some functions. */
static struct node *request_node(struct node *n);
static struct node *request_node_specific(int id, int type);
static struct node *request_node_any(int type);

/* Create a node and initialize it. */
static void create_nodes(int type, int num, int num_children)
{
	/* Initialize the lookup tables for this node type. */
	num_nodes[type] = num;
	node_lookup[type] = malloc(num * sizeof(struct node));

	/* Initialize all fields of each node. */
	for (int i = 0; i < num; i++) {
		struct node *n = &node_lookup[type][i];
		n->id = i;
		n->type = type;
		n->refcount = 0;
		n->score = 0;
		CIRCLEQ_INSERT_TAIL(&node_list[type], n, link);
		n->parent = NULL;
		n->children = malloc(num_children * sizeof(struct node*));
		for (int j = 0; j < num_children; j++) {
			n->children[j] = &node_lookup[child_node_type(type)]
			                             [j + i * num_children];
			n->children[j]->parent = n;
		}
		n->num_children = num_children;

	}
}

/* Build our available nodes structure. */
void nodes_init()
{
	create_nodes(CORE, num_cores, 0);
	create_nodes(CHIP, num_chips, cores_per_chip);
	create_nodes(SOCKET, num_sockets, chips_per_socket);
	create_nodes(NUMA, num_numa, sockets_per_numa);
}

/* Update the score of every node in the topology. */
static void update_scores()
{
	struct node *n = NULL;
	for (int i = NUMA; i >= CORE; i--) {
		for (int j = 0; j < num_nodes[i]; j++) {
			n = &node_lookup[i][j];
			if (n->parent == NULL)
				n->score = n->refcount;
			else
				n->score = n->refcount + n->parent->score;
		}
	}
}

/* Returns the best node to allocate in case request_any_node() is called. */
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
	n->refcount++;
	if (n->parent != NULL)
		incref_node_recursive(n->parent);
}

/* Recursively decref a node from its level through its ancestors. */
static void decref_node_recursive(struct node *n)
{
	n->refcount--;
	if (n->parent != NULL)
		decref_node_recursive(n->parent);
}

/* Request a specific node by node type. */
static struct node *request_node(struct node *n)
{
	if (n == NULL)
		return NULL;
	if (n->refcount)
		return NULL;

	if (n->num_children == 0) {
		incref_node_recursive(n);
		update_scores();
	}
	for (int i = 0; i < n->num_children; i++)
		request_node(n->children[i]);
	return n;
}

/* Yield a specific node by node type. */
static int yield_node(struct node *n)
{
	if (n == NULL)
		return -1;
	if (n->refcount != 1)
		return -1;

	decref_node_recursive(n);
	return 0;
}

/* Request for any node of a given type. */
static struct node *request_node_any(int type)
{
	return request_node(find_best_node(type)); 
}

/* Request a specific node by id. */
static struct node *request_node_specific(int type, int id)
{
	return request_node(&node_lookup[type][id]);
}

/* Reinsert a specific node by id. */
static int yield_node_specific(int type, int id)
{
	return yield_node(&node_lookup[type][id]);
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

/* Yields the specified core back to the system. */
int yield_core_specific(int core_id)
{
	return yield_node_specific(CORE, core_id);
}

void print_resources()
{		
	struct node *np = NULL;
	CIRCLEQ_FOREACH(np, &node_list[CORE], link) {
		printf("core id: %d, refcount: %d, score: %d, parent_id: %d, ",
		       np->id, np->refcount, np->score, np->parent->id);
		printf("parent_type: %d,next_core_available id: %d\n",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&node_list[CORE],
					 np, link)->id);
	}
	
	np = NULL;
	CIRCLEQ_FOREACH(np, &node_list[CHIP], link) {
		printf("chip id: %d, refcount: %d, score: %d, parent_id: %d, ",
		       np->id, np->refcount, np->score, np->parent->id);
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
		printf("socket id: %d, refcount: %d, score: %d, parent_id: %d, ",
		       np->id, np->refcount, np->score, np->parent->id);
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
		printf("numa id: %d, refcount: %d, score: %d",
		       np->id, np->refcount,np->score);
		for (int i = 0; i< sockets_per_numa; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
}

