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

/* Allocate core i of the system. Return !0 if success */
int allocate_spec_core(int i) 
{
	struct node *np = request_core_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any core of the system. Return !0 if success */
int allocate_any_core() 
{
	struct node *np = request_core_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield core i of the system. Return 0 if success. */
int yield_spec_core(int i) 
{
	return yield_core_specific(i);
}

/* Allocate chip i of the system. Return !0 if success */
int allocate_spec_chip(int i) 
{
	struct node *np = request_chip_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any chip of the system. Return !0 if success */
int allocate_any_chip() 
{
	struct node *np = request_chip_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield all the chips of the system. Return 0 if success. */
int yield_spec_chip(int i) 
{
	int ret =0;
	for (int j = 0 ; j<cores_per_chip; j++)
		ret += yield_core_specific(i*cores_per_chip+j);
	return ret;
}

/* Allocate socket i of the system. Return !0 if success */
int allocate_spec_socket(int i) 
{
	struct node *np = request_socket_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any socket of the system. Return !0 if success */
int allocate_any_socket() 
{
	struct node *np = request_socket_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield all the sockets of the system. Return 0 if success. */
int yield_spec_socket(int i) 
{
	int ret =0;
	for (int j = 0 ; j<cores_per_socket; j++)
		ret += yield_core_specific(i*cores_per_socket+j);
	return ret;
}

/* Allocate numa i of the system. Return !0 if success */
int allocate_spec_numa(int i) 
{
	struct node *np = request_numa_specific(i);
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Allocate any numa of the system. Return !0 if success */
int allocate_any_numa() 
{
	struct node *np = request_numa_any();
	if (np != NULL)
		return np->refcount;
	else 
		return 0;
}

/* Yield all the numas of the system. Return 0 if success. */
int yield_spec_numa(int i) 
{
	int ret =0;
	for (int j = 0 ; j<cores_per_numa; j++)
		ret += yield_core_specific(i*cores_per_numa+j);
	return ret;
}

void alloc_yield_cores_specific()
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
		if (yield_spec_core(i) != 0) {
			printf("yield_spec_core: Error when yielding a core\n");
			return;
		}
/* Trying to re yield the same core twice shouldn't work */
		if (yield_spec_core(i) > -1) {
			printf("yield_spec_core: Error when re-yielding a core\n");
			return;
		}
	}
}

void alloc_yield_cores_any()
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
		if (yield_spec_core(i) != 0) {
			printf("yield_spec_core: Error when yielding a core\n");
			return;
		}
/* Trying to re yield the same core twice shouldn't work */
		if (yield_spec_core(i) > -1) {
			printf("yield_spec_core: Error when re-yielding a core\n");
			return;
		}
	}
}

void alloc_yield_chips_specific()
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
		if (yield_spec_chip(i) != 0) {
			printf("yield_spec_chip: Error when yielding a chip\n");
			return;
		}
/* Trying to re yield the same chip twice shouldn't work */
		if (yield_spec_chip(i) > -1) {
			printf("yield_spec_chip: Error when re-yielding a chip\n");
			return;
		}
	}
}

void alloc_yield_chips_any()
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
		if (yield_spec_chip(i) != 0) {
			printf("yield_spec_chip: Error when yielding a chip\n");
			return;
		}
/* Trying to re yield the same chip twice shouldn't work */
		if (yield_spec_chip(i) > -1) {
			printf("yield_spec_chip: Error when re-yielding a chip\n");
			return;
		}
	}
}

void alloc_yield_sockets_specific()
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
		if (yield_spec_socket(i) != 0) {
			printf("yield_spec_socket: Error when yielding a socket\n");
			return;
		}
/* Trying to re yield the same socket twice shouldn't work */
		if (yield_spec_socket(i) > -1) {
			printf("yield_spec_socket: Error when re-yielding a socket\n");
			return;
		}
	}
}

void alloc_yield_sockets_any()
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
		if (yield_spec_socket(i) != 0) {
			printf("yield_spec_socket: Error when yielding a socket\n");
			return;
		}
/* Trying to re yield the same socket twice shouldn't work */
		if (yield_spec_socket(i) > -1) {
			printf("yield_spec_socket: Error when re-yielding a socket\n");
			return;
		}
	}
}

void alloc_yield_numas_specific()
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
		if (yield_spec_numa(i) != 0) {
			printf("yield_spec_numa: Error when yielding a numa\n");
			return;
		}
/* Trying to re yield the same numa twice shouldn't work */
		if (yield_spec_numa(i) > -1) {
			printf("yield_spec_numa: Error when re-yielding a numa\n");
			return;
		}
	}
}

void alloc_yield_numas_any()
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
		if (yield_spec_numa(i) != 0) {
			printf("yield_spec_numa: Error when yielding a numa\n");
			return;
		}
/* Trying to re yield the same numa twice shouldn't work */
		if (yield_spec_numa(i) > -1) {
			printf("yield_spec_numa: Error when re-yielding a numa\n");
			return;
		}
	}
}

/* Allocate and yield successively all cores, chips, sockets and numa of the system */
void test_aloc_yield_all_typeofnodes()
{
	alloc_yield_cores_specific();
	alloc_yield_cores_any();
	alloc_yield_chips_specific();
	alloc_yield_chips_any();
	alloc_yield_sockets_specific();
	alloc_yield_sockets_any();
	alloc_yield_numas_specific();
	alloc_yield_numas_any();
}

void test_schedule()
{
	test_aloc_yield_all_typeofnodes();
	
}
