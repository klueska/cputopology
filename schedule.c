#include <stdio.h>
#include <stdlib.h>
#include "schedule.h"


struct thread_list runnable_threads = CIRCLEQ_HEAD_INITIALIZER(runnable_threads); 
struct core_list runnable_cores = CIRCLEQ_HEAD_INITIALIZER(runnable_cores);
struct socket_list runnable_sockets = CIRCLEQ_HEAD_INITIALIZER(runnable_sockets);
struct numa_list runnable_numas = CIRCLEQ_HEAD_INITIALIZER(runnable_numas);

void init_numas()
{
	struct node *first = malloc(sizeof(struct node));
	first->sons = (struct node **)malloc(
		NUMAS_IN_SYSTEM*sizeof(struct node*));
	first->id = 0;
	first->type = NUMA;
/*Add our numas to the list of available numas*/
	CIRCLEQ_INSERT_TAIL(&runnable_numas, first, numa_link);
/*Link our numa to NULL (no father)*/
	first->father = NULL;
/*Fil our numa lookup array*/
	numas_lookup[0] = first;
}

void init_sockets()
{
	struct node *first = malloc(sizeof(struct node));
	first->sons = (struct node **)malloc(
		SOCKETS_PER_NUMA*sizeof(struct node *));
	first->id = 0;
	first->type = SOCKET;
	/* struct node *second = malloc(sizeof(struct node)); */
	/* second->id = 1; */
/*Add our sockets to the list of available sockets*/
	CIRCLEQ_INSERT_TAIL(&runnable_sockets, first, socket_link);
	/* CIRCLEQ_INSERT_TAIL(&runnable_sockets, second, socket_link); */

/*Link our sockets to their numa parent*/
	first->father = find_numa(0);
/*Link the parents (numas) of each sockets*/
	find_numa(0)->sons[0] = first;
/*Fil our socket lookup array*/
	sockets_lookup[0] = first;
}
void init_cores()
{
	struct node *first = malloc(sizeof(struct node));
	first->sons = (struct node **)malloc(
		CORES_PER_SOCKET*sizeof(struct node*));
	first->id = 0;
	first->type = CORE;
	struct node *second = malloc(sizeof(struct node));
	second->sons = (struct node **)malloc(
		CORES_PER_SOCKET*sizeof(struct node*));
	second->id = 1;
	second->type = CORE;
        /* struct node *third = malloc(sizeof(struct node)); */
	/* third->id = 2; */
	/* struct node *fourth = malloc(sizeof(struct node)); */
	/* third->id = 3; */

/*Add our cores to the list of available cores*/
	CIRCLEQ_INSERT_TAIL(&runnable_cores, first, core_link);
	CIRCLEQ_INSERT_TAIL(&runnable_cores, second, core_link);
	/* CIRCLEQ_INSERT_TAIL(&runnable_cores, third, core_link); */
	/* CIRCLEQ_INSERT_TAIL(&runnable_cores, fourth, core_link); */
/*Link our cores to their socket parent*/
	first->father = find_socket(0);
	second->father = find_socket(0);
/*Link the parents (sockets) of each cores*/
	find_socket(0)->sons[0] = first;
	find_socket(0)->sons[1] = second;
/*Fil our socket lookup array*/
	cores_lookup[0] = first;
	cores_lookup[1] = second;
}


void init_threads()
{
	struct node *first = malloc(sizeof(struct node));
	first->id = 0;
	first->type = THREAD;
	struct node *second = malloc(sizeof(struct node));
	second->id = 1;
	second->type = THREAD;
	struct node *third = malloc(sizeof(struct node));
	third->id = 2;
	third->type = THREAD;
	struct node *fourth = malloc(sizeof(struct node));
	fourth->id = 3;
	fourth->type = THREAD;

/*Add our thread to the list of available threads*/
	CIRCLEQ_INSERT_TAIL(&runnable_threads, first, thread_link);
	CIRCLEQ_INSERT_TAIL(&runnable_threads, second, thread_link);
	CIRCLEQ_INSERT_TAIL(&runnable_threads, third, thread_link);
	CIRCLEQ_INSERT_TAIL(&runnable_threads, fourth, thread_link);	
/*Link our threads to their core parent*/
	first->father = find_core(0);
	second->father = find_core(0);
	third->father = find_core(1);
	fourth->father = find_core(1);
/*Link the parents (cores) of each threads*/
	find_core(0)->sons[0] = first;
	find_core(0)->sons[1] = second;
	find_core(1)->sons[0] = third;
	find_core(1)->sons[1] = fourth;
/*Fil our socket lookup array*/
	threads_lookup[0] = first;
	threads_lookup[1] = second;
	threads_lookup[2] = third;
	threads_lookup[3] = fourth;
}

struct node *find_thread(int threadid)
{
	/*Find our node walking through the list*/
	
	/* for (struct node *np = runnable_threads.cqh_first; np != (void *)&runnable_threads; */
	/*      np = np->thread_link.cqe_next) { */
	/* 	if (np->id == threadid) { */
	/* 		return np; */
	/* 	} */
	/* } */
	/* return NULL; */

	/*Find our node using the lookups arrays*/
	return threads_lookup[threadid];
}

struct node *find_core(int coreid)
{
	/* for (struct node *np = runnable_cores.cqh_first; np != (void *)&runnable_cores; */
	/*      np = np->core_link.cqe_next) { */
	/* 	if (np->id == coreid) { */
	/* 		return np; */
	/* 	} */
	/* } */
	/* return NULL; */
	return cores_lookup[coreid];
}

struct node *find_socket(int socketid)
{
	/* for (struct node *np = runnable_sockets.cqh_first; np != (void *)&runnable_sockets; */
	/*      np = np->socket_link.cqe_next) { */
	/* 	if (np->id == socketid) { */
	/* 		return np; */
	/* 	} */
	/* } */
	/* return NULL; */
		return sockets_lookup[socketid];
}

struct node *find_numa(int numaid)
{
	/* for (struct node *np = runnable_numas.cqh_first; np != (void *)&runnable_numas; */
	/*      np = np->numa_link.cqe_next) { */
	/* 	if (np->id == numaid) { */
	/* 		return np; */
	/* 	} */
	/* } */
	/* return NULL; */
	return numas_lookup[numaid];
}


void print_val()
{
	init_numas();
	init_sockets();
	init_cores();
	init_threads();
		
	for (struct node *np = runnable_threads.cqh_first; np != (void *)&runnable_threads;
	     np = np->thread_link.cqe_next) {
		printf("\n thread id: %d, type: %d, father_id: %d, father_type: %d\n",
		       np->id, np->type, np->father->id, np->father->type);		
	}
	for (struct node *np = runnable_cores.cqh_first; np != (void *)&runnable_cores;
	     np = np->core_link.cqe_next) {
		printf("\n core id: %d, type: %d, father_id: %d, father_type: %d",
		       np->id, np->type, np->father->id, np->father->type);
		for(int i = 0; i< THREADS_PER_CORE; i++)
			printf(", son %d type: %d",
			       np->sons[i]->id, np->sons[i]->type);
		printf("\n");
	}
	for (struct node *np = runnable_sockets.cqh_first; np != (void *)&runnable_sockets;
	     np = np->socket_link.cqe_next) {
		printf("\n socket id: %d, type: %d, father_id: %d, father_type: %d",
		       np->id, np->type, np->father->id, np->father->type);
		for(int i = 0; i< CORES_PER_SOCKET; i++)
			printf(", son %d type: %d",
			       np->sons[i]->id, np->sons[i]->type);
		printf("\n");
	}
	for (struct node *np = runnable_numas.cqh_first; np != (void *)&runnable_numas;
	     np = np->numa_link.cqe_next) {
		printf("\n numa id: %d, type: %d", np->id, np->type);
		for(int i = 0; i< SOCKETS_PER_NUMA; i++)
			printf(", son %d type: %d",
			       np->sons[i]->id, np->sons[i]->type);
		printf("\n");
	}
}
