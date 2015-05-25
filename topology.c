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
#include <stdio.h>
#include <sched.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch.h"
#include "acpi.h"
#include "topology.h"

struct cpu_topology cpu_topology[MAX_NUM_CPUS] = { [0 ... (MAX_NUM_CPUS-1) ]
                                                   {-1, -1, -1, -1, false} };
int os_coreid_lookup[MAX_NUM_CPUS] = {[0 ... (MAX_NUM_CPUS - 1)] -1};
int hw_coreid_lookup[MAX_NUM_CPUS] = {[0 ... (MAX_NUM_CPUS - 1)] -1};
int num_cores = 0;
int num_chips = 0;
int num_sockets = 0;
int num_numa = 0;
int sockets_per_numa = 0;
int chips_per_socket = 0;
int cores_per_chip = 0;

static void adjust_ids(int id_offset)
{
	int new_id = 0, old_id = -1;
	for (int i=0; i<MAX_NUM_CPUS; i++) {
		for (int j=0; j<MAX_NUM_CPUS; j++) {
			int *id_field = ((void*)&cpu_topology[j] + id_offset);
			if (*id_field >= new_id) {
				if (old_id == -1)
					old_id = *id_field;
				if (old_id == *id_field)
					*id_field = new_id;
			}
		}
		old_id = -1;
		new_id++;
	}
}

void fill_topology_lookup_maps()
{
	int last_numa = -1, last_socket = -1, last_cpu = -1;
	for (int i=0; i<MAX_NUM_CPUS; i++) {
		if (cpu_topology[i].online) {
			if (cpu_topology[i].numa_id > last_numa) {
				last_numa = cpu_topology[i].numa_id;
				num_numa++;
			}
			if (cpu_topology[i].socket_id > last_socket) {
				last_socket = cpu_topology[i].socket_id;
				num_sockets++;
			}
			if (cpu_topology[i].chip_id > last_cpu) {
				last_cpu = cpu_topology[i].chip_id;
				num_chips++;
			}
			hw_coreid_lookup[num_cores] = i;
			os_coreid_lookup[i] = num_cores;
			num_cores++;
		}
	}
	num_sockets *= num_numa;
	num_chips *= num_sockets;
}

void build_ressources_structure(){
	build_structure(num_numa, num_sockets, 
			chips_per_socket, cores_per_chip);
}

static void build_topology(uint32_t core_bits, uint32_t chip_bits)
{
	cores_per_chip = (1 << core_bits);
	int cores_per_socket = (1 << chip_bits);
	chips_per_socket = cores_per_socket / cores_per_chip;
	int num_cores = (1 << (core_bits + chip_bits));
	uint32_t apic_id = 0, core_id = 0, chip_id = 0, socket_id = 0;

	int i = 0;
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic) {
			apic_id = temp->lapic.id;
			socket_id = apic_id & ~(num_cores - 1);
			chip_id = (apic_id >> core_bits) & (cores_per_socket - 1);
			core_id = apic_id & (cores_per_chip - 1);

			/* TODO: Build numa topology properly */
			cpu_topology[apic_id].numa_id = 0;
			cpu_topology[apic_id].socket_id = socket_id;
			cpu_topology[apic_id].chip_id = chip_id;
			cpu_topology[apic_id].core_id = core_id;
			cpu_topology[apic_id].online = false;
			i++;
		}
		temp = temp->next;
	}
	adjust_ids(offsetof(struct cpu_topology, socket_id));
	adjust_ids(offsetof(struct cpu_topology, chip_id));
	adjust_ids(offsetof(struct cpu_topology, core_id));
}

static void build_flat_topology()
{
	int i = 0;
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic) {
			int apic_id = temp->lapic.id;
			cpu_topology[apic_id].numa_id = 0;
			cpu_topology[apic_id].socket_id = 0;
			cpu_topology[apic_id].chip_id = 0;
			cpu_topology[apic_id].core_id = 0;
			cpu_topology[apic_id].online = false;
		}
		temp = temp->next;
	}
}

void topology_init()
{
	uint32_t eax, ebx, ecx, edx;
	int smt_leaf, core_leaf;
	uint32_t core_bits = 0, chip_bits = 0;

	eax = 0x0000000b;
	ecx = 1;
	cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
	core_leaf = (ecx >> 8) & 0x00000002;
	if (core_leaf == 2) {
		chip_bits = eax;
		eax = 0x0000000b;
		ecx = 0;
		cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
		smt_leaf = (ecx >> 8) & 0x00000001;
		if (smt_leaf == 1) {
			core_bits = eax;
			chip_bits = chip_bits - core_bits;
		}
	}
	if (chip_bits)
		build_topology(core_bits, chip_bits);
	else 
		build_flat_topology();
}

int numa_domain()
{
	struct cpu_topology *c = &cpu_topology[get_apic_id()];
	return c->numa_id;
}

int socketid()
{
	struct cpu_topology *c = &cpu_topology[get_apic_id()];
	return num_sockets/num_numa * numa_domain() +
	       c->socket_id;
}

int chipid()
{
	struct cpu_topology *c = &cpu_topology[get_apic_id()];
	return num_chips/num_sockets * socketid() +
	       c->chip_id;
}

int coreid()
{
	struct cpu_topology *c = &cpu_topology[get_apic_id()];
	return num_cores/num_chips * chipid() +
	       c->core_id;
}

void print_cpu_topology() 
{
	printf("num_numa: %d, num_sockets: %d, num_chips: %d, num_cores: %d\n",
            num_numa, num_sockets, num_chips, num_cores);
	for (int i=0; i < num_cores; i++) {
		int coreid = hw_coreid_lookup[i];
		printf("OScoreid: %3d, HWcoreid: %3d, Numa Domain: %3d, "
		       "Socket: %3d, Chip: %3d, Core: %3d\n",
		       i,
		       coreid,
		       cpu_topology[coreid].numa_id, 
		       cpu_topology[coreid].socket_id,
		       cpu_topology[coreid].chip_id, 
		       cpu_topology[coreid].core_id);
	}
}

