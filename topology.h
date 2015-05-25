/*
 * Copyright (c) 20015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * See LICENSE for details.
 */

#ifndef TOPOLOGY_H_
#define TOPOLOGY_H_

#include <stdbool.h>
#include "schedule.h"

#define MAX_NUM_CPUS 256

struct cpu_topology {
	int numa_id;
	int socket_id;
	int chip_id;
	int core_id;
	bool online;
};

extern struct cpu_topology cpu_topology[MAX_NUM_CPUS];

int numa_domain();
int socketid();
int chipid();
int coreid();
int get_sockets_per_numa();
int get_chips_per_socket();
int get_cores_per_chip();

void topology_init();
void fill_topology_lookup_maps();
void print_cpu_topology();
void build_available_ressources();
#endif /* !TOPOLOGY_H_ */
