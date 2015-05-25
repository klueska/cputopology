#include <stdio.h>
#include <stdlib.h>
#include "schedule.h"


struct core_list runnable_cores = CIRCLEQ_HEAD_INITIALIZER(runnable_cores); 
struct chip_list runnable_chips = CIRCLEQ_HEAD_INITIALIZER(runnable_chips);
struct socket_list runnable_sockets = CIRCLEQ_HEAD_INITIALIZER(runnable_sockets);
struct numa_list runnable_numas = CIRCLEQ_HEAD_INITIALIZER(runnable_numas);

void init_cores(int chip_id, int cores_per_chip)
{
	for( int i = 0; i< cores_per_chip; i++) {
	struct node *new_core = malloc(sizeof(struct node));
	new_core->id = i + chip_id * cores_per_chip;
	new_core->type = CORE;
/*Add our core to the list of available cores*/
	CIRCLEQ_INSERT_TAIL(&runnable_cores, new_core, core_link);
/*Link our cores to their chip parent*/
	new_core->father = find_chip(chip_id);
/*Link the parents (chips) of each cores*/
	find_chip(chip_id)->sons[i] = new_core;
/*Fil our socket lookup array*/
	cores_lookup[new_core->id] = new_core;
	}
}

void init_chips(int socket_id, int chips_per_socket,
		int cores_per_chip)
{
	for( int i = 0; i< chips_per_socket; i++) {
		struct node *new_chip = malloc(sizeof(struct node));
		new_chip->sons = (struct node **)malloc(
			chips_per_socket*sizeof(struct node*));
		new_chip->id = i + socket_id * chips_per_socket;
		new_chip->type = CHIP;
/*Add our chips to the list of available chips*/
	CIRCLEQ_INSERT_TAIL(&runnable_chips, new_chip, chip_link);
/*Link our chips to their socket parent*/
	new_chip->father = find_socket(socket_id);
/*Link the parents (sockets) of each chips*/
	find_socket(socket_id)->sons[i] = new_chip;
/*Fil our socket lookup array*/
	chips_lookup[new_chip->id] = new_chip;
	init_cores(new_chip->id, cores_per_chip);
	}
}

void init_sockets(int numa_id, int sockets_per_numa,
		  int chips_per_socket, int cores_per_chip)
{
	for( int i = 0; i< sockets_per_numa; i++) {
		struct node *new_socket = malloc(sizeof(struct node));
		new_socket->sons = (struct node **)malloc(
			sockets_per_numa*sizeof(struct node *));
		new_socket->id = i + numa_id * sockets_per_numa;
		new_socket->type = SOCKET;
/*Add our sockets to the list of available sockets*/
		CIRCLEQ_INSERT_TAIL(&runnable_sockets, new_socket, socket_link);
/*Link our sockets to their numa parent*/
		new_socket->father = find_numa(numa_id);
/*Link the parents (numas) of each sockets*/
		find_numa(numa_id)->sons[i] = new_socket;
/*Fil our socket lookup array*/
		sockets_lookup[new_socket->id] = new_socket;
		init_chips(new_socket->id, chips_per_socket, cores_per_chip);
	}
}

void build_structure_ressources(int nb_numas, int sockets_per_numa,
		int chips_per_socket, int cores_per_chip)
{
	for( int i = 0; i< nb_numas; i++) {
		struct node *new_numa = malloc(sizeof(struct node));
		new_numa->sons = (struct node **)malloc(
			nb_numas*sizeof(struct node*));
		new_numa->id = i;
		new_numa->type = NUMA;
/*Add our numas to the list of available numas*/
		CIRCLEQ_INSERT_TAIL(&runnable_numas, new_numa, numa_link);
/*Link our numa to NULL (no father)*/
		new_numa->father = NULL;
/*Fil our numa lookup array*/
		numas_lookup[i] = new_numa;
		init_sockets(new_numa->id, sockets_per_numa, 
			     chips_per_socket, cores_per_chip);
	}
}

struct node *find_core(int coreid)
{
	return cores_lookup[coreid];
}

struct node *find_chip(int chipid)
{
	return chips_lookup[chipid];
}

struct node *find_socket(int socketid)
{
	return sockets_lookup[socketid];
}

struct node *find_numa(int numaid)
{
	return numas_lookup[numaid];
}

void print_available_ressources()
{		
	struct node *np = runnable_cores.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_cores, core_link) {
		printf("core id: %d, type: %d, father_id: %d, ",
		       np->id, np->type, np->father->id);
		printf("father_type: %d,next_core_available id: %d\n",
		       np->father->type, 
		       CIRCLEQ_LOOP_NEXT(&runnable_cores,
					 np, core_link)->id);
	}
	
	np = runnable_chips.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_chips, chip_link) {
		printf("chip id: %d, type: %d, father_id: %d, ",
		       np->id, np->type, np->father->id);
		printf("father_type: %d,next_chip_available id: %d",
		       np->father->type, 
		       CIRCLEQ_LOOP_NEXT(&runnable_chips,
					 np, chip_link)->id);
		for(int i = 0; np->sons[i] == NULL; i++)
			printf(", son %d type: %d",
			       np->sons[i]->id, np->sons[i]->type);
		printf("\n");
	}
	
	np = runnable_sockets.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_sockets, socket_link) {
		printf("socket id: %d, type: %d, father_id: %d, ",
		       np->id, np->type, np->father->id);
		printf("father_type: %d,next_socket_available id: %d",
		       np->father->type, 
		       CIRCLEQ_LOOP_NEXT(&runnable_sockets,
					 np, socket_link)->id);
		for(int i = 0; np->sons[i] == NULL; i++)
			printf(", son %d type: %d",
			       np->sons[i]->id, np->sons[i]->type);
		printf("\n");
	}
	
	np = runnable_numas.cqh_first;
	CIRCLEQ_FOREACH(np, &runnable_numas, numa_link) {
		printf("numa id: %d, type: %d, next_numa_available id: %d",
		       np->id, np->type,
		       CIRCLEQ_LOOP_NEXT(&runnable_numas,
					 np, numa_link)->id);
		for(int i = 0; np->sons[i] == NULL; i++)
			printf(", son %d type: %d",
			       np->sons[i]->id, np->sons[i]->type);
		printf("\n");
	}
}
