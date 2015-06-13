/*
 * Copyright (c) 2015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * Kevin Klues <klueska@cs.berkeley.edu>
 * See LICENSE for details.
 */

#ifndef	SCHEDULE_H
#define	SCHEDULE_H

#include <sys/queue.h>

enum node_type { CORE, CHIP, SOCKET, NUMA };
static char node_label[4][7] = { "CORE", "CHIP", "SOCKET", "NUMA" };
#define NUM_NODE_TYPES sizeof(enum node_type)
#define child_node_type(t) ((t) - 1)

struct node {
	int id;
	int owner_pid;
	int provisioned;
	enum node_type type;
	int refcount[NUM_NODE_TYPES];
	struct node *parent;
	struct node *children;
	STAILQ_ENTRY(node) link;
};

STAILQ_HEAD(core_list, node);

void nodes_init();
struct core_list alloc_numa_any(int amt);
struct core_list alloc_numa_specific(int numa_id);
struct core_list alloc_socket_any(int amt);
struct core_list alloc_socket_specific(int socket_id);
struct core_list alloc_chip_any(int amt);
struct core_list alloc_chip_specific(int chip_id);
struct core_list alloc_core_any(int amt);
struct core_list alloc_core_specific(int core_id);
int free_core_specific(int core_id);

void print_node(struct node *n);
void print_nodes(int type);
void print_all_nodes();
void test_structure();

#endif
