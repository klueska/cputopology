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
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch.h"
#include "acpi.h"
#include "topology.h"

struct topology_info cpu_topology_info;
int *os_coreid_lookup;
int *hw_coreid_lookup;

#define num_cores           (cpu_topology_info.num_cores)
#define num_cores_power2    (cpu_topology_info.num_cores_power2)
#define num_chips           (cpu_topology_info.num_chips)
#define num_sockets         (cpu_topology_info.num_sockets)
#define num_numa            (cpu_topology_info.num_numa)
#define cores_per_numa      (cpu_topology_info.cores_per_numa)
#define cores_per_socket    (cpu_topology_info.cores_per_socket)
#define cores_per_chip      (cpu_topology_info.cores_per_chip)
#define chips_per_socket    (cpu_topology_info.chips_per_socket)
#define sockets_per_numa    (cpu_topology_info.sockets_per_numa)
#define core_list           (cpu_topology_info.core_list)

static void adjust_ids(int id_offset)
{
	int new_id = 0, old_id = -1;
	for (int i=0; i<num_cores_power2; i++) {
		for (int j=0; j<num_cores_power2; j++) {
			int *id_field = ((void*)&core_list[j] + id_offset);
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
	int last_numa = -1, last_socket = -1, last_chip = -1, last_core = -1;
	for (int i=0; i<num_cores_power2; i++) {
		if (core_list[i].online) {
			if (core_list[i].numa_id > last_numa) {
				last_numa = core_list[i].numa_id;
				num_numa++;
		 	}
			if (core_list[i].socket_id > last_socket) {
				last_socket = core_list[i].socket_id;
				sockets_per_numa++;
			}
			if (core_list[i].chip_id > last_chip) {
				last_chip = core_list[i].chip_id;
				chips_per_socket++;
			}
			if (core_list[i].core_id > last_core) {
				last_core = core_list[i].core_id;
				cores_per_chip++;
			}
			hw_coreid_lookup[num_cores] = i;
			os_coreid_lookup[i] = num_cores;
			num_cores++;
		}
	}
	cores_per_socket = chips_per_socket * cores_per_chip;
	cores_per_numa = sockets_per_numa * cores_per_socket;
	num_sockets = sockets_per_numa * num_numa;
	num_chips = chips_per_socket * num_sockets;
}

static void init_topology_info()
{
	core_list = calloc(num_cores_power2, sizeof(struct core_info));
	os_coreid_lookup = calloc(num_cores_power2, sizeof(int));
	hw_coreid_lookup = calloc(num_cores_power2, sizeof(int));
}

static void build_topology(uint32_t core_bits, uint32_t chip_bits)
{
	int max_cores_per_chip = (1 << core_bits);
	int max_cores_per_socket = (1 << chip_bits);
	num_cores_power2 = (1 << (core_bits + chip_bits));

	init_topology_info();

	uint32_t apic_id = 0, core_id = 0, chip_id = 0, socket_id = 0;
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic) {
			apic_id = temp->lapic.id;
			socket_id = apic_id & ~(num_cores_power2 - 1);
			chip_id = (apic_id >> core_bits) & (max_cores_per_socket - 1);
			core_id = apic_id & (max_cores_per_chip - 1);

			/* TODO: Build numa topology properly */
			core_list[apic_id].numa_id = 0;
			core_list[apic_id].socket_id = socket_id;
			core_list[apic_id].chip_id = chip_id;
			core_list[apic_id].core_id = core_id;
			core_list[apic_id].online = false;
		}
		temp = temp->next;
	}
	adjust_ids(offsetof(struct core_info, socket_id));
	adjust_ids(offsetof(struct core_info, chip_id));
	adjust_ids(offsetof(struct core_info, core_id));
}

static void build_flat_topology()
{
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic) {
			int apic_id = temp->lapic.id;
			core_list[apic_id].numa_id = 0;
			core_list[apic_id].socket_id = 0;
			core_list[apic_id].chip_id = 0;
			core_list[apic_id].core_id = 0;
			core_list[apic_id].online = false;
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
	struct core_info *c = &core_list[get_apic_id()];
	return c->numa_id;
}

int socketid()
{
	struct core_info *c = &core_list[get_apic_id()];
	return num_sockets/num_numa * numa_domain() + c->socket_id;
}

int chipid()
{
	struct core_info *c = &core_list[get_apic_id()];
	return num_chips/num_sockets * socketid() + c->chip_id;
}

int coreid()
{
	struct core_info *c = &core_list[get_apic_id()];
	return num_cores/num_chips * chipid() + c->core_id;
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
		       core_list[coreid].numa_id,
		       core_list[coreid].socket_id,
		       core_list[coreid].chip_id,
		       core_list[coreid].core_id);
	}
}

