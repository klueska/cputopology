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
static void *core_proxy(void *arg)
{
	int core = (int)(long)arg;
	pin_to_core(core);

	pthread_mutex_lock(&mutex);
	/* printf("numa_domain: %3d, socketid: %3d, chipid: %3d, coreid: %3d\n", */
	/*        numa_domain(), socket_id(), chip_id(), core_id()); */
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
	acpiinit();	
	topology_init();
	nodes_init();
	test_id_funcs();
	//test_schedule();
	test_structure();
	return 0;
}

