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

#include "test_schedule.h"


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Allocate core i of the system. Return !0 if success */
int allocate_spec_core(int i) 
{
	struct node *np = alloc_core_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any core of the system. Return !0 if success */
int allocate_any_core() 
{
	struct node *np = alloc_core_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield core i of the system. Return 0 if success. */
int free_spec_core(int i) 
{
	return free_core_specific(i);
}

/* Allocate chip i of the system. Return !0 if success */
int allocate_spec_chip(int i) 
{
	struct node *np = alloc_chip_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any chip of the system. Return !0 if success */
int allocate_any_chip() 
{
	struct node *np = alloc_chip_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield all the chips of the system. Return 0 if success. */
int free_spec_chip(int i) 
{
	int ret =0;
	for (int j = 0 ; j<cores_per_chip; j++)
		ret += free_core_specific(i*cores_per_chip+j);
	return ret;
}

/* Allocate socket i of the system. Return !0 if success */
int allocate_spec_socket(int i) 
{
	struct node *np = alloc_socket_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any socket of the system. Return !0 if success */
int allocate_any_socket() 
{
	struct node *np = alloc_socket_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield all the sockets of the system. Return 0 if success. */
int free_spec_socket(int i) 
{
	int ret =0;
	for (int j = 0 ; j<cores_per_socket; j++)
		ret += free_core_specific(i*cores_per_socket+j);
	return ret;
}

/* Allocate numa i of the system. Return !0 if success */
int allocate_spec_numa(int i) 
{
	struct node *np = alloc_numa_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any numa of the system. Return !0 if success */
int allocate_any_numa() 
{
	struct node *np = alloc_numa_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield all the numas of the system. Return 0 if success. */
int free_spec_numa(int i) 
{
	int ret =0;
	for (int j = 0 ; j<cores_per_numa; j++)
		ret += free_core_specific(i*cores_per_numa+j);
	return ret;
}

void alloc_free_cores_specific()
{
/* Allocate all the cores of the system. */
	for (int i = 0; i< num_cores; i++) {
		if (allocate_spec_core(i) == 0) {
			printf("allocate_spec_core: Error when allocating a core\n");
			return;
		}
/* Trying to re allocate the same core twice shouldn't work */
		if (allocate_spec_core(i) != 0) {
			printf("allocate_spec_core: Error when re-allocating a core\n");
			return;
		}
	}

/* Yield all the cores of the system. */
	for (int i = 0; i< num_cores; i++) {
		if (free_spec_core(i) != 0) {
			printf("free_spec_core: Error when yielding a core\n");
			return;
		}
/* Trying to re yield the same core twice shouldn't work */
		if (free_spec_core(i) > -1) {
			printf("free_spec_core: Error when re-yielding a core\n");
			return;
		}
	}
}

void alloc_free_cores_any()
{
/* Allocate all the cores of the system. */
	for (int i = 0; i< num_cores; i++) {
		if (allocate_any_core() == 0) {
			printf("allocate_any_core: Error when allocating a core\n");
			return;
		}
	}

/* Yield all the cores of the system. */
	for (int i = 0; i< num_cores; i++) {
		if (free_spec_core(i) != 0) {
			printf("free_spec_core: Error when yielding a core\n");
			return;
		}
/* Trying to re yield the same core twice shouldn't work */
		if (free_spec_core(i) > -1) {
			printf("free_spec_core: Error when re-yielding a core\n");
			return;
		}
	}
}

void alloc_free_chips_specific()
{
/* Allocate all the chips of the system. */
	for (int i = 0; i< num_chips; i++) {
		if (allocate_spec_chip(i) == 0) {
			printf("allocate_spec_chip: Error when allocating a chip\n");
			return;
		}
/* Trying to re allocate the same chip twice shouldn't work */
		if (allocate_spec_chip(i) != 0) {
			printf("allocate_spec_chip: Error when re-allocating a chip\n");
			return;
		}
	}

/* Yield all the chips of the system. */
	for (int i = 0; i< num_chips; i++) {
		if (free_spec_chip(i) != 0) {
			printf("free_spec_chip: Error when yielding a chip\n");
			return;
		}
/* Trying to re yield the same chip twice shouldn't work */
		if (free_spec_chip(i) > -1) {
			printf("free_spec_chip: Error when re-yielding a chip\n");
			return;
		}
	}
}

void alloc_free_chips_any()
{
/* Allocate all the chips of the system. */
	for (int i = 0; i< num_chips; i++) {
		if (allocate_any_chip(i) == 0) {
			printf("allocate_any_chip: Error when allocating a chip\n");
			return;
		}
	}

/* Yield all the chips of the system. */
	for (int i = 0; i< num_chips; i++) {
		if (free_spec_chip(i) != 0) {
			printf("free_spec_chip: Error when yielding a chip\n");
			return;
		}
/* Trying to re yield the same chip twice shouldn't work */
		if (free_spec_chip(i) > -1) {
			printf("free_spec_chip: Error when re-yielding a chip\n");
			return;
		}
	}
}

void alloc_free_sockets_specific()
{
/* Allocate all the sockets of the system. */
	for (int i = 0; i< num_sockets; i++) {
		if (allocate_spec_socket(i) == 0) {
			printf("allocate_spec_socket: Error when allocating a socket\n");
			return;
		}
/* Trying to re allocate the same socket twice shouldn't work */
		if (allocate_spec_socket(i) != 0) {
			printf("allocate_spec_socket: Error when re-allocating a socket\n");
			return;
		}
	}

/* Yield all the sockets of the system. */
	for (int i = 0; i< num_sockets; i++) {
		if (free_spec_socket(i) != 0) {
			printf("free_spec_socket: Error when yielding a socket\n");
			return;
		}
/* Trying to re yield the same socket twice shouldn't work */
		if (free_spec_socket(i) > -1) {
			printf("free_spec_socket: Error when re-yielding a socket\n");
			return;
		}
	}
}

void alloc_free_sockets_any()
{
/* Allocate all the sockets of the system. */
	for (int i = 0; i< num_sockets; i++) {
		if (allocate_any_socket(i) == 0) {
			printf("allocate_any_socket: Error when allocating a socket\n");
			return;
		}
	}

/* Yield all the sockets of the system. */
	for (int i = 0; i< num_sockets; i++) {
		if (free_spec_socket(i) != 0) {
			printf("free_spec_socket: Error when yielding a socket\n");
			return;
		}
/* Trying to re yield the same socket twice shouldn't work */
		if (free_spec_socket(i) > -1) {
			printf("free_spec_socket: Error when re-yielding a socket\n");
			return;
		}
	}
}

void alloc_free_numas_specific()
{
/* Allocate all the numas of the system. */
	for (int i = 0; i< num_numa; i++) {
		if (allocate_spec_numa(i) == 0) {
			printf("allocate_spec_numa: Error when allocating a numa\n");
			return;
		}
/* Trying to re allocate the same numa twice shouldn't work */
		if (allocate_spec_numa(i) != 0) {
			printf("allocate_spec_numa: Error when re-allocating a numa\n");
			return;
		}
	}

/* Yield all the numas of the system. */
	for (int i = 0; i< num_numa; i++) {
		if (free_spec_numa(i) != 0) {
			printf("free_spec_numa: Error when yielding a numa\n");
			return;
		}
/* Trying to re yield the same numa twice shouldn't work */
		if (free_spec_numa(i) > -1) {
			printf("free_spec_numa: Error when re-yielding a numa\n");
			return;
		}
	}
}

void alloc_free_numas_any()
{
/* Allocate all the numas of the system. */
	for (int i = 0; i< num_numa; i++) {
		if (allocate_any_numa(i) == 0) {
			printf("allocate_any_numa: Error when allocating a numa\n");
			return;
		}
	}

/* Yield all the numas of the system. */
	for (int i = 0; i< num_numa; i++) {
		if (free_spec_numa(i) != 0) {
			printf("free_spec_numa: Error when yielding a numa\n");
			return;
		}
/* Trying to re yield the same numa twice shouldn't work */
		if (free_spec_numa(i) > -1) {
			printf("free_spec_numa: Error when re-yielding a numa\n");
			return;
		}
	}
}

static void *alloc_core_all(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);
	pthread_mutex_lock(&mutex);
	//printf("I am thread %d\n",core);
	alloc_free_cores_specific();
	alloc_free_cores_any();
	alloc_free_chips_specific();
	alloc_free_chips_any();
	alloc_free_sockets_specific();
	alloc_free_sockets_any();
	alloc_free_numas_specific();
	alloc_free_numas_any();
	pthread_mutex_unlock(&mutex);
}

static void *alloc_core3(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);
	pthread_mutex_lock(&mutex);
	struct node *n = alloc_core_specific(3);
	printf("I am thread %d and ",core);
	if( n!= NULL)
		printf("I got core %d\n", n->id);
	else
		printf("I can't get core %d\n", 3);
	pthread_mutex_unlock(&mutex);
}

static void *alloc_chip1(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);
	pthread_mutex_lock(&mutex);
	struct node *n = alloc_chip_specific(1);
	printf("I am thread %d and ",core);
	if( n!= NULL)
		printf("I got chip %d\n", n->id);
	else
		printf("I can't get chip %d\n", 1);
	pthread_mutex_unlock(&mutex);
}

static void *alloc_chip2(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);
	pthread_mutex_lock(&mutex);
	struct node *n = alloc_chip_specific(2);
	printf("I am thread %d and ",core);
	if( n!= NULL)
		printf("I got chip %d\n", n->id);
	else
		printf("I can't get chip %d\n", 2);
	pthread_mutex_unlock(&mutex);
}

static void *alloc_socket0(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);
	pthread_mutex_lock(&mutex);
	struct node *n = alloc_socket_specific(0);
	printf("I am thread %d and ",core);
	if( n!= NULL)
		printf("I got socket %d\n", n->id);
	else
		printf("I can't get socket %d\n", 0);
	pthread_mutex_unlock(&mutex);
}

/* Successively allocate and yield all cores, chips, sockets and numa of the  */
/* system. Here we do either specific and any allocation on a 4-threaded */
/* progam. */
void test_schedule_dynamic()
{
	int nb_threads = 4;
	pthread_t pthread[nb_threads];
	for (int i=0; i<nb_threads; i++) {
		pthread_create(&pthread[i], NULL, alloc_core_all, (void*)(long)i);
	}
	for (int i=0; i<nb_threads; i++) {
		pthread_join(pthread[i], NULL);
	}

}

/* Successively allocate and yield all cores, chips, sockets and numa of the  */
/* system. Here we do either specific and any allocation on a single threaded */
/* progam. */
void test_schedule_static()
{
	alloc_free_cores_specific();
	alloc_free_cores_any();
	alloc_free_chips_specific();
	alloc_free_chips_any();
	alloc_free_sockets_specific();
	alloc_free_sockets_any();
	alloc_free_numas_specific();
	alloc_free_numas_any();
}

/* For now ask for core3, chip1, chip2 and socket0. We can see if everything is  */
/* all right thanks to the print. Not the best option though.  */
void test_schedule_dynamic_exp()
{
	int nb_threads = 4;
	pthread_t pthread[nb_threads];
	pthread_create(&pthread[0], NULL, alloc_core3, (void*)(long)0);
	pthread_create(&pthread[1], NULL, alloc_chip1, (void*)(long)1);
	pthread_create(&pthread[2], NULL, alloc_chip2, (void*)(long)2);
	pthread_create(&pthread[3], NULL, alloc_socket0, (void*)(long)3);
	for (int i=0; i<nb_threads; i++) {
		pthread_join(pthread[i], NULL);
	}

}

void test_schedule()
{
	test_schedule_static();
	test_schedule_dynamic();
	test_schedule_dynamic_exp();
}
