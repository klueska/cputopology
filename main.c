/*
 * Copyright (c) 2015 The Regents of the University of California
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

#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <pthread.h>
#include <stdio.h>
#include "arch.h"
#include "acpi.h"
#include "topology.h"
#include "schedule.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
extern struct core_list runnable_cores; 
extern struct chip_list runnable_chips; 
extern struct socket_list runnable_sockets; 
extern struct numa_list runnable_numas; 
static void *core_proxy(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);

	pthread_mutex_lock(&mutex);
	printf("numa_domain: %3d, socketid: %3d, chipid: %3d, coreid: %3d\n",
	       numa_domain(), socketid(), chipid(), coreid());
	pthread_mutex_unlock(&mutex);
}

void test_id_funcs()
{
	int ncpus = get_nprocs();
	pthread_t pthread[ncpus];

	for (int i=0; i<ncpus; i++) {
		pthread_create(&pthread[i], NULL, core_proxy, (void*)(long)i);
	}
	for (int i=0; i<ncpus; i++) {
		pthread_join(pthread[i], NULL);
	}
}

int main(int argc, char **argv)
{
	int nb_numas = 1, sockets_per_numa = 2,
		cores_per_socket = 4, threads_per_core = 2; 
	acpiinit();	
	topology_init();
	archinit();
	fill_topology_lookup_maps();
	build_structure_ressources(num_numa, num_sockets, 
				   chips_per_socket, cores_per_chip);
	test_id_funcs();
	//print_available_ressources();
	

	//struct node *numa1 = request_numa_id(0); 
	//struct node *socket1 = request_any_socket();
	//struct node *core1 = request_core_id(1);
	//struct node *chip1 = request_any_chip();
	


	/* if(core1 == NULL) */
	/* 	printf("The core you required is no longer available\n"); */
	/* else{ */
	/* 	printf("core id: %d, type: %d, father_id: %d, ", */
	/* 	       core1->id, core1->type, core1->father->id); */
	/* 	printf("father_type: %d,next_core_available id: %d\n", */
	/* 	       core1->father->type, */
	/* 	       CIRCLEQ_LOOP_NEXT(&runnable_cores, */
	/* 				 core1, core_link)->id); */
	/* } */
	
	/* if(chip1 == NULL) */
	/* 	printf("The chip you required is no longer available\n"); */
	/* else{ */
	/* 	printf("chip id: %d, type: %d, father_id: %d, ", */
	/* 	       chip1->id, chip1->type, chip1->father->id); */
	/* 	printf("father_type: %d,next_chip_available id: %d", */
	/* 	       chip1->father->type, */
	/* 	       CIRCLEQ_LOOP_NEXT(&runnable_chips, */
	/* 				 chip1, chip_link)->id); */
	/* 	for(int i = 0 ; chip1->sons[i] != NULL; i++) */
	/* 		printf(", son %d type: %d", */
	/* 		       chip1->sons[i]->id, chip1->sons[i]->type); */
	/* 	printf("\n"); */
	/* } */

	/* if(socket1 == NULL) */
	/* 	printf("The socket you required is no longer available\n"); */
	/* else{ */
	/* 	printf("socket id: %d, type: %d, father_id: %d, ", */
	/* 	       socket1->id, socket1->type, socket1->father->id); */
	/* 	printf("father_type: %d,next_socket_available id: %d", */
	/* 	       socket1->father->type, */
	/* 	       CIRCLEQ_LOOP_NEXT(&runnable_sockets, */
	/* 				 socket1, socket_link)->id); */
	/* 	for(int i = 0 ; socket1->sons[i] != NULL; i++) */
	/* 		printf(", son %d type: %d", */
	/* 		       socket1->sons[i]->id, socket1->sons[i]->type); */
	/* 	printf("\n"); */
	/* } */
	
	/* if(numa1 == NULL) */
	/* 	printf("The numa you required is no longer available\n"); */
	/* else{ */
	/* 	printf("numa id: %d, type: %d,  ", */
	/* 	       numa1->id, numa1->type); */
	/* 	printf("next_numa_available id: %d", */
	/* 	       CIRCLEQ_LOOP_NEXT(&runnable_numas, */
	/* 				 numa1, numa_link)->id); */
	/* 	for(int i = 0 ; numa1->sons[i] != NULL; i++) */
	/* 		printf(", son %d type: %d", */
	/* 		       numa1->sons[i]->id, numa1->sons[i]->type); */
	/* 	printf("\n"); */
	/* } */
	/* return 0; */
}

