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
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "arch.h"
#include "acpi.h"
#include "topology.h"

struct topology_info cpu_topology_info;
int *os_coreid_lookup;

#define num_cores           (cpu_topology_info.num_cores)
#define num_cpus            (cpu_topology_info.num_cpus)
#define num_sockets         (cpu_topology_info.num_sockets)
#define num_numa            (cpu_topology_info.num_numa)
#define cores_per_numa      (cpu_topology_info.cores_per_numa)
#define cores_per_socket    (cpu_topology_info.cores_per_socket)
#define cores_per_cpu       (cpu_topology_info.cores_per_cpu)
#define cpus_per_socket     (cpu_topology_info.cpus_per_socket)
#define sockets_per_numa    (cpu_topology_info.sockets_per_numa)
#define max_apic_id         (cpu_topology_info.max_apic_id)
#define core_list           (cpu_topology_info.core_list)

static void adjust_ids(int id_offset)
{
	int new_id = 0, old_id = -1;
	for (int i = 0; i < num_cores; i++) {
		for (int j = 0; j < num_cores; j++) {
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

static void set_num_cores()
{
	/* Figure out the maximum number of cores we actually have and set it in
	 * our cpu_topology_info struct. */
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic)
			num_cores++;
		temp = temp->next;
	}
}

static void set_max_apic_id() {
	/* Figure out what the max apic_id we will ever have is and set it in our
	 * cpu_topology_info struct. */
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic) {
			if (temp->lapic.id > max_apic_id)
				max_apic_id = temp->lapic.id;
		}
		temp = temp->next;
	}
}

static void init_os_coreid_lookup() {
	/* Allocate (max_apic_id+1) entries in our os_coreid_lookup table.
	 * There may be holes in this table because of the way apic_ids work, but
	 * a little wasted space is OK for a constant time lookup of apic_id ->
	 * logical core id (from the os's perspective). Memset the array to -1 to
	 * to represent invalid entries (which it's very possible we might have if
	 * the apic_id space has holes in it).  */
	os_coreid_lookup = malloc((max_apic_id + 1) * sizeof(int));
	memset(os_coreid_lookup, -1, (max_apic_id + 1) * sizeof(int));

	/* Loop through and set all valid entries to 0 to start with (making them
	 * temporarily valid, but not yet set to the correct value). This step is
	 * necessary because there is no ordering to the linked list we are
	 * pulling these ids from. After this, loop back through and set the
	 * mapping appropriately. */
	struct Apicst *temp = apics->st;
	while (temp) {
		if (temp->type == ASlapic)
			os_coreid_lookup[temp->lapic.id] = 0;
		temp = temp->next;
	}
	int os_coreid = 0;
	for (int i = 0; i <= max_apic_id; i++)
		if (os_coreid_lookup[i] == 0)
			os_coreid_lookup[i] = os_coreid++;
}

static void init_core_list(uint32_t core_bits, uint32_t cpu_bits)
{
	/* Assuming num_cpus and max_apic_id have been set, we can allocate our
	 * core_list to the proper size. Initialize all entries to 0s to being
	 * with. */
	core_list = calloc(num_cores, sizeof(struct core_info));

	/* Loop through all possible apic_ids and fill in the core_list array with
	 * *relative* topology info. We will change this relative info to absolute
	 * info in a future step. As part of this step, we update our
	 * os_coreid_lookup array to contain the proper value. */
	int os_coreid = 0;
	int max_cpus = (1 << cpu_bits);
	int max_cores_per_cpu = (1 << core_bits);
	int max_logical_cores = (1 << (core_bits + cpu_bits));
	uint32_t core_id = 0, cpu_id = 0, socket_id = 0;
	for (int apic_id = 0; apic_id <= max_apic_id; apic_id++) {
		if (os_coreid_lookup[apic_id] != -1) {
			socket_id = apic_id & ~(max_logical_cores - 1);
			cpu_id = (apic_id >> core_bits) & (max_cpus - 1);
			core_id = apic_id & (max_cores_per_cpu - 1);

			/* TODO: Build numa topology properly */
			core_list[os_coreid].numa_id = 0;
			core_list[os_coreid].socket_id = socket_id;
			core_list[os_coreid].cpu_id = cpu_id;
			core_list[os_coreid].core_id = core_id;
			core_list[os_coreid].apic_id = apic_id;
			os_coreid++;
		}
	}

	/* The various id's set in the previous step are all unique in terms of
	 * representing the topology (i.e. all cores under the same socket have
	 * the same socket_id set), but these id's are not necessarily contiguous,
	 * and are only relative to the level of the hierarchy they exist at (e.g.
	 * cpu_id 4 may exist under *both* socke_id 0 and socket_id 1). In this
	 * step, we Squash these id's down so they are contiguous. In a following
	 * step, we will make them all absolute instead of relative. */
	adjust_ids(offsetof(struct core_info, socket_id));
	adjust_ids(offsetof(struct core_info, cpu_id));
	adjust_ids(offsetof(struct core_info, core_id));
}

static void init_core_list_flat()
{
	/* Assuming num_cpus and max_apic_id have been set, we can allocate our
	 * core_list to the proper size. Initialize all entries to 0s to being
	 * with. */
	core_list = calloc(num_cores, sizeof(struct core_info));

	/* Loop through all possible apic_ids and fill in the core_list array with
	 * flat topology info. */
	int os_coreid = 0;
	for (int apic_id = 0; apic_id <= max_apic_id; apic_id++) {
		if (os_coreid_lookup[apic_id] != -1) {
			/* TODO: Build numa topology properly */
			core_list[os_coreid].numa_id = 0;
			core_list[os_coreid].socket_id = 0;
			core_list[os_coreid].cpu_id = 0;
			core_list[os_coreid].core_id = os_coreid;
			core_list[os_coreid].apic_id = apic_id;
			os_coreid++;
		}
	}
}

static void set_remaining_topology_info()
{
	/* Assuming we have our core_list set up with relative topology info, loop
	 * through our core_list and calculate the other statistics that we hold
	 * in our cpu_topology_info struct. */
	int last_numa = -1, last_socket = -1, last_cpu = -1, last_core = -1;
	for (int i = 0; i < num_cores; i++) {
		if (core_list[i].numa_id > last_numa) {
			last_numa = core_list[i].numa_id;
			num_numa++;
		}
		if (core_list[i].socket_id > last_socket) {
			last_socket = core_list[i].socket_id;
			sockets_per_numa++;
		}
		if (core_list[i].cpu_id > last_cpu) {
			last_cpu = core_list[i].cpu_id;
			cpus_per_socket++;
		}
		if (core_list[i].core_id > last_core) {
			last_core = core_list[i].core_id;
			cores_per_cpu++;
		}
	}
	cores_per_socket = cpus_per_socket * cores_per_cpu;
	cores_per_numa = sockets_per_numa * cores_per_socket;
	num_sockets = sockets_per_numa * num_numa;
	num_cpus = cpus_per_socket * num_sockets;
}

static void update_core_list_with_absolute_ids()
{
	/* Fix up our core_list to have absolute id's at every level. */
	for (int i = 0; i < num_cores; i++) {
		struct core_info *c = &core_list[i];
		c->socket_id = num_sockets/num_numa * c->numa_id + c->socket_id;
		c->cpu_id = num_cpus/num_sockets * c->socket_id + c->cpu_id;
		c->core_id = num_cores/num_cpus * c->cpu_id + c->core_id;
	}
}

static void build_topology(uint32_t core_bits, uint32_t cpu_bits)
{
	set_num_cores();
	set_max_apic_id();
	init_os_coreid_lookup();
	init_core_list(core_bits, cpu_bits);
	set_remaining_topology_info();
	update_core_list_with_absolute_ids();
}

static void build_flat_topology()
{
	set_num_cores();
	set_max_apic_id();
	init_os_coreid_lookup();
	init_core_list_flat();
	set_remaining_topology_info();
}

void topology_init()
{
	uint32_t eax, ebx, ecx, edx;
	int smt_leaf, core_leaf;
	uint32_t core_bits = 0, cpu_bits = 0;

	eax = 0x0000000b;
	ecx = 1;
	cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
	core_leaf = (ecx >> 8) & 0x00000002;
	if (core_leaf == 2) {
		cpu_bits = eax;
		eax = 0x0000000b;
		ecx = 0;
		cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
		smt_leaf = (ecx >> 8) & 0x00000001;
		if (smt_leaf == 1) {
			core_bits = eax;
			cpu_bits = cpu_bits - core_bits;
		}
	}
	if (cpu_bits)
		build_topology(core_bits, cpu_bits);
	else 
		build_flat_topology();
}

int numa_domain()
{
	int os_coreid = os_coreid_lookup[get_apic_id()];
	return core_list[os_coreid].numa_id;
}

int socket_id()
{
	int os_coreid = os_coreid_lookup[get_apic_id()];
	return core_list[os_coreid].socket_id;
}

int cpu_id()
{
	int os_coreid = os_coreid_lookup[get_apic_id()];
	return core_list[os_coreid].cpu_id;
}

int core_id()
{
	int os_coreid = os_coreid_lookup[get_apic_id()];
	return core_list[os_coreid].core_id;
}

void print_cpu_topology() 
{
	printf("num_numa: %d, num_sockets: %d, num_cpus: %d, num_cores: %d\n",
	       num_numa, num_sockets, num_cpus, num_cores);
	for (int i = 0; i < num_cores; i++) {
		printf("OScoreid: %3d, HWcoreid: %3d, Numa Domain: %3d, "
		       "Socket: %3d, Cpu: %3d, Core: %3d\n",
		       i,
		       core_list[i].apic_id,
		       core_list[i].numa_id,
		       core_list[i].socket_id,
		       core_list[i].cpu_id,
		       core_list[i].core_id);
	}
}

