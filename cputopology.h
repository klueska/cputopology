/*
 * Copyright (c) 20015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * See LICENSE for details.
 */

#ifndef TOPOLOGY_H_
#define TOPOLOGY_H_

#include <stdint.h>

#define MAX_NUM_CPUS 256

struct cpu_topology {
	int numa_id;
	int socket_id;
	int cpu_id;
	int core_id;
	bool online;
};

extern struct cpu_topology cpu_topology[MAX_NUM_CPUS];

#endif /* !TOPOLOGY_H_ */
