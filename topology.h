/*
 * Copyright (c) 20015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * See LICENSE for details.
 */

#ifndef TOPOLOGY_H_
#define TOPOLOGY_H_

#include <stdbool.h>
#include "schedule.h"

struct core_info {
	int numa_id;
	int socket_id;
	int cpu_id;
	int core_id;
	bool online;
};

struct topology_info {
	int num_cores;
	int num_cpus;
	int num_sockets;
	int num_numa;
	int cores_per_cpu;
	int cores_per_socket;
	int cores_per_numa;
	int cpus_per_socket;
	int sockets_per_numa;
	int max_logical_cores;
	struct core_info *core_list;
};

extern struct topology_info cpu_topology_info;
extern int *os_coreid_lookup;
extern int *hw_coreid_lookup;

int numa_domain();
int socket_id();
int cpu_id();
int core_id();

void topology_init();
void fill_topology_lookup_maps();
void print_cpu_topology();
#endif /* !TOPOLOGY_H_ */
