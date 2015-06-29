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
#include <sched.h>
#include <pthread.h>
#include <stdint.h>
#include <malloc.h>
#include <stdbool.h>
#include "arch.h"
#include "topology.h"

void pin_to_core(int coreid)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(coreid, &cpuset);
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	sched_yield();
}

uint32_t get_apic_id()
{
	uint32_t eax, ebx, ecx, edx;
	eax = 0x0000000b;
	ecx = 0;
	cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
	return edx;
}

static void *core_proxy(void *arg)
{
	int coreid = (int)(long)arg;
	pin_to_core(coreid);

	uint32_t apic_id = get_apic_id();
	cpu_topology_info.core_list[apic_id].online = true;
}

void archinit()
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

