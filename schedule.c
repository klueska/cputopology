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

/* A list type to create our list of runnable cores, sockets, etc. */
CIRCLEQ_HEAD(node_list, node);
struct node_list runnable_cores = CIRCLEQ_HEAD_INITIALIZER(runnable_cores);
struct node_list runnable_chips = CIRCLEQ_HEAD_INITIALIZER(runnable_chips);
struct node_list runnable_sockets = CIRCLEQ_HEAD_INITIALIZER(runnable_sockets);
struct node_list runnable_numas = CIRCLEQ_HEAD_INITIALIZER(runnable_numas);

/* Here are our 4 arrays of numas, sockets, chips and cores. */
/* The index is the id of the considerated array. */
static struct node **numa_lookup;
static struct node **socket_lookup;
static struct node **chip_lookup;
static struct node **core_lookup;

/* static struct node *find_node(int id, enum node_type type) */
/* { */
/* 	switch (type) { */
/* 	case CORE: */
/* 		return find_core(id); */
/* 	case CHIP: */
/* 		return find_chip(id); */
/* 	case SOCKET: */
/* 		return find_socket(id); */
/* 	case NUMA: */
/* 		return find_numa(id); */
/* 	} */
/* 	return NULL; */
/* } */

static struct node *find_numa(int numaid)
{
	return numa_lookup[numaid];
}
static struct node *find_socket(int socketid)
{
	return socket_lookup[socketid];
}
static struct node *find_chip(int chipid)
{
	return chip_lookup[chipid];
}
static struct node *find_core(int coreid)
{
	return core_lookup[coreid];
}

/* static void init_test(int nb_numas, int sockets_per_numa, enum node_type type */
/* 		      int chips_per_socket, int cores_per_chip) */
/* { */
/* 	int counter = 0;//cores_per_chip, chips_per_socket, sockets_per_numa	 */
/* 	int parent_id = 0; */
/* 	struct node *new_core = malloc(sizeof(struct node)); */
/* 	new_core->id = i + chip_id * cores_per_chip; */
/* 	new_core->type = CORE; */
/* 	new_core->available = true; */
	
/* 	switch (type) { */
/* 	case CORE: */
/* 		core_lookup = (struct node **)malloc( */
/* 			num_cores*sizeof(struct node*)); */
/* 		counter = cores_per_chip; */
/* 		parent_id = chip_id; */
/* 		break; */
/* 	case CHIP: */
/* 		lookup_array = chip_lookup; */
/* 		chip_lookup = (struct node **)malloc( */
/* 			num_chips*sizeof(struct node*)); */
/* 		parent_id = socket_id; */
/* 		break; */
/* 	case SOCKET: */
/* 		socket_lookup = (struct node **)malloc( */
/* 			num_sockets*sizeof(struct node*)); */
/* 		counter = sockets_per_numa; */
/* 		parent_id = numa_id; */
/* 		break; */
/* 	case NUMA: */
/* 		numa_lookup = (struct node **)malloc( */
/* 			num_numa*sizeof(struct node*)); */
/* 		break; */
/* 	} */
/* 	return NULL; */
/* 	for (int i = 0; i< cores_per_chip; i++) { */

/* /\* Add our core to the list of available cores *\/ */
/* 		CIRCLEQ_INSERT_TAIL(&runnable_cores, new_core, link); */
/* /\* Link our cores to their chip parent *\/ */
/* 		new_core->parent = find_chip(chip_id); */
/* /\* Link the parents (chips) of each cores *\/ */
/* 		find_chip(chip_id)->children[i] = new_core; */
/* /\* Fil our socket lookup array *\/ */
/* 		core_lookup[new_core->id] = new_core; */
/* 	} */
/* } */

/* Initialize all the cores of our structure. */
static void init_cores(int chip_id, int cores_per_chip)
{
	for (int i = 0; i< cores_per_chip; i++) {
		struct node *new_core = malloc(sizeof(struct node));
		new_core->id = i + chip_id * cores_per_chip;
		new_core->type = CORE;
		new_core->available = true;
/* Add our core to the list of available cores */
		CIRCLEQ_INSERT_TAIL(&runnable_cores, new_core, link);
/* Link our cores to their chip parent */
		new_core->parent = find_chip(chip_id);
/* Link the parents (chips) of each cores */
		find_chip(chip_id)->children[i] = new_core;
/* Fil our socket lookup array */
		core_lookup[new_core->id] = new_core;
	}
}

/* Initialize all the chips of our structure. */
static void init_chips(int socket_id, int chips_per_socket,
		       int cores_per_chip)
{
	for( int i = 0; i< chips_per_socket; i++) {
		struct node *new_chip = malloc(sizeof(struct node));
		new_chip->children = (struct node **)malloc(
			cores_per_chip*sizeof(struct node*));
		new_chip->id = i + socket_id * chips_per_socket;
		new_chip->type = CHIP;
		new_chip->available = true;
/* Add our chips to the list of available chips */
		CIRCLEQ_INSERT_TAIL(&runnable_chips, new_chip, link);
/* Link our chips to their socket parent */
		new_chip->parent = find_socket(socket_id);
/* Link the parents (sockets) of each chips */
		find_socket(socket_id)->children[i] = new_chip;
/* Fil our socket lookup array */
		chip_lookup[new_chip->id] = new_chip;
		init_cores(new_chip->id, cores_per_chip);
	}
}

/* Initialize all the sockets of our structure. */
static void init_sockets(int numa_id, int sockets_per_numa,
			 int chips_per_socket, int cores_per_chip)
{
	for (int i = 0; i< sockets_per_numa; i++) {
		struct node *new_socket = malloc(sizeof(struct node));
		printf("CHIPS PER SOCKETS %d\n",chips_per_socket);
		new_socket->children = (struct node **)malloc(
			chips_per_socket*sizeof(struct node *));
		new_socket->id = i + numa_id * sockets_per_numa;
		new_socket->type = SOCKET;
		new_socket->available = true;
/* Add our sockets to the list of available sockets */
		CIRCLEQ_INSERT_TAIL(&runnable_sockets, new_socket, link);
/* Link our sockets to their numa parent */
		new_socket->parent = find_numa(numa_id);
/* Link the parents (numas) of each sockets */
		find_numa(numa_id)->children[i] = new_socket;
/* Fil our socket lookup array */
		socket_lookup[new_socket->id] = new_socket;
		init_chips(new_socket->id, chips_per_socket, cores_per_chip);
	}
}

/* Build our available resources structure. Here we initialize the numas and */
/* call init_sockets, init_chips and init_cores. */
void resources_init()
{
	numa_lookup = (struct node **)malloc(
		num_numa*sizeof(struct node*));
	socket_lookup = (struct node **)malloc(
		num_sockets*sizeof(struct node*));
	chip_lookup = (struct node **)malloc(
		num_chips*sizeof(struct node*));
	core_lookup = (struct node **)malloc(
		num_cores*sizeof(struct node*));

	for (int i = 0; i< num_numa; i++) {
		struct node *new_numa = malloc(sizeof(struct node));
		new_numa->children = (struct node **)malloc(
			sockets_per_numa*sizeof(struct node*));
		new_numa->id = i;
		new_numa->type = NUMA;
		new_numa->available = true;
/* Add our numas to the list of available numas */
		CIRCLEQ_INSERT_TAIL(&runnable_numas, new_numa, link);
/* Link our numa to NULL (no parent) */
		new_numa->parent = NULL;
/* Fil our numa lookup array */
		numa_lookup[i] = new_numa;
		init_sockets(new_numa->id, sockets_per_numa, 
			     chips_per_socket, cores_per_chip);
	}
}

/* This fucntion is always called by a core. It removes all the  */
/* calling core's parent (chips, socket and num) from their available list. */
static void remove_parent(struct node *node)
{
	if (node->parent !=NULL)
		remove_parent(node->parent);
	node->available = false;
	switch (node->type) {
	case CORE:
		CIRCLEQ_REMOVE(&runnable_cores, node, link);
		break;
	case CHIP:
		CIRCLEQ_REMOVE(&runnable_chips, node, link);
		break;	
	case SOCKET:
		CIRCLEQ_REMOVE(&runnable_sockets, node, link);
		break;
	case NUMA:
		CIRCLEQ_REMOVE(&runnable_numas, node, link);
		break;
	}
}

/* Returns the first available numa, returns NULL if nothing is availbale */
struct node *request_numa_any()
{
	if (CIRCLEQ_EMPTY(&runnable_numas))
		return NULL;
	struct node *first_numa = CIRCLEQ_FIRST(&runnable_numas); 

	if (first_numa->available) {
		first_numa->available = false;
		for(int i = 0; i< sockets_per_numa; i++) {
			request_socket_specific(first_numa->children[i]->id);
		}
		CIRCLEQ_REMOVE(&runnable_numas, first_numa, link);
		return first_numa;
	} else {
		return NULL;
	}
}

/* Returns the numa with the id in input if available, return NULL otherwise */
struct node *request_numa_specific(int numa_id)
{
	struct node *first_numa = numa_lookup[numa_id];
	if (first_numa->available) {
		first_numa->available = false;
		for (int i = 0;  i< sockets_per_numa; i++) {
			request_socket_specific(first_numa->children[i]->id);
		}
		CIRCLEQ_REMOVE(&runnable_numas, first_numa, link);
		return first_numa;
	} else {
		return NULL;
	}
}

/* Returns the first available socket, returns NULL if nothing is availbale */
struct node *request_socket_any()
{
	if (CIRCLEQ_EMPTY(&runnable_sockets))
		return NULL;
	struct node *first_socket = CIRCLEQ_FIRST(&runnable_sockets); 
	if (first_socket->available) {
		first_socket->available = false;
		for (int i = 0;  i< chips_per_socket; i++) {
			request_chip_specific(first_socket->children[i]->id);
		}
		CIRCLEQ_REMOVE(&runnable_sockets, first_socket, link);
		return first_socket;
	} else {
		return NULL;
	}
}
		
/* Returns the socket with the id in input if available, return NULL otherwise */
struct node *request_socket_specific(int socket_id)
{
	struct node *first_socket = socket_lookup[socket_id];
	if (first_socket->available) {
		first_socket->available = false;
		for (int i = 0; i< chips_per_socket; i++) {
			request_chip_specific(first_socket->children[i]->id);
		}
		CIRCLEQ_REMOVE(&runnable_sockets, first_socket, link);
		return first_socket;
	} else {
		return NULL;
	}
}

/* Returns the first available chip, returns NULL if nothing is availbale */
struct node *request_chip_any()
{
	if (CIRCLEQ_EMPTY(&runnable_chips))
		return NULL;
	struct node *first_chip = CIRCLEQ_FIRST(&runnable_chips); 
	if (first_chip->available) {
		first_chip->available = false;
		for (int i = 0; i< cores_per_chip; i++) {
			request_core_specific(first_chip->children[i]->id);
		}
		CIRCLEQ_REMOVE(&runnable_chips, first_chip, link);
		return first_chip;
	} else {
		return NULL;
	}
}

/* Returns the chip with the id in input if available, return NULL otherwise */
/* call request_core(core_id) on child so we remove them from the available list */
struct node *request_chip_specific(int chip_id)
{
	struct node *first_chip = chip_lookup[chip_id];
	if (first_chip->available) {
		first_chip->available = false;
		for (int i = 0; i< cores_per_chip; i++) {
			request_core_specific(first_chip->children[i]->id);
		}
		CIRCLEQ_REMOVE(&runnable_chips, first_chip, link);
		return first_chip;
	} else {
		return NULL;
	}
}

/* Returns the first available core, returns NULL if nothing is availbale */
struct node *request_core_any()
{
	if (CIRCLEQ_EMPTY(&runnable_cores))
		return NULL;
	struct node *first_core = CIRCLEQ_FIRST(&runnable_cores); 
	if (first_core->available) {
		first_core->available = false;
		CIRCLEQ_REMOVE(&runnable_cores, first_core, link);
		remove_parent(first_core);
		return first_core;
	} else {
		return NULL;
	}
}

/* Returns the core with the id in input if available, return NULL otherwise */
struct node *request_core_specific(int core_id)
{
	struct node *first_core = core_lookup[core_id];
	if (first_core->available) {
		first_core->available = false;
		CIRCLEQ_REMOVE(&runnable_cores, first_core, link);
		remove_parent(first_core);
		return first_core;
	} else {
		return NULL;
	}
}


void print_available_resources()
{		
	struct node *np = runnable_cores.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_cores, link) {
		printf("core id: %d, type: %d, parent_id: %d, ",
		       np->id, np->type, np->parent->id);
		printf("parent_type: %d,next_core_available id: %d\n",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&runnable_cores,
					 np, link)->id);
	}
	
	np = runnable_chips.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_chips, link) {
		printf("chip id: %d, type: %d, parent_id: %d, ",
		       np->id, np->type, np->parent->id);
		printf("parent_type: %d,next_chip_available id: %d",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&runnable_chips,
					 np, link)->id);
		for (int i = 0; i< cores_per_chip; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
	
	np = runnable_sockets.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_sockets, link) {
		printf("socket id: %d, type: %d, parent_id: %d, ",
		       np->id, np->type, np->parent->id);
		printf("parent_type: %d,next_socket_available id: %d",
		       np->parent->type, 
		       CIRCLEQ_LOOP_NEXT(&runnable_sockets,
					 np, link)->id);
		for (int i = 0;  i< chips_per_socket; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
	
	np = runnable_numas.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_numas, link) {
		printf("numa id: %d, type: %d, next_numa_available id: %d",
		       np->id, np->type,
		       CIRCLEQ_LOOP_NEXT(&runnable_numas,
					 np, link)->id);
		for (int i = 0; i< sockets_per_numa; i++)
			printf(", son %d type: %d",
			       np->children[i]->id, np->children[i]->type);
		printf("\n");
	}
}

