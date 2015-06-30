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
	int chip_id;
	int core_id;
	bool online;
};

struct topology_info {
	int num_cores;
	int num_cores_power2;
	int num_chips;
	int num_sockets;
	int num_numa;
	int cores_per_numa;
	int cores_per_socket;
	int cores_per_chip;
	int chips_per_socket;
	int sockets_per_numa;
	struct core_info *core_list;
};

extern struct topology_info cpu_topology_info;
extern int *os_coreid_lookup;
extern int *hw_coreid_lookup;

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
#endif /* !TOPOLOGY_H_ */
