/*
 * Copyright (c) 2015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/queue.h>
#include "topology.h" 

#ifndef	SCHEDULE_H
#define	SCHEDULE_H

enum node_type { CORE, CHIP, SOCKET, NUMA };

struct node {
	int id;
	enum node_type type;
	bool available;
	CIRCLEQ_ENTRY(node) link;
	struct node *parent;
	struct node **children;
};

void resources_init();
struct node *request_numa_any();
struct node *request_numa_specific(int numa_id);
struct node *request_socket_any();
struct node *request_socket_specific(int socket_id);
struct node *request_chip_any();
struct node *request_chip_specific(int chip_id);
struct node *request_core_any();
struct node *request_core_specific(int core_id);
void print_available_resources();

#endif
