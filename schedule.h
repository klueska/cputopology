/*
 * Copyright (c) 2015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * Kevin Klues <klueska@cs.berkeley.edu>
 * See LICENSE for details.
 */

#ifndef	SCHEDULE_H
#define	SCHEDULE_H

#include <sys/queue.h>

enum node_type { CORE, CPU, SOCKET, NUMA };
static char node_label[4][7] = { "CORE", "CPU", "SOCKET", "NUMA" };
#define NUM_NODE_TYPES sizeof(enum node_type)

struct node {
	int id;
	enum node_type type;
	int refcount[NUM_NODE_TYPES];
	struct node *parent;
	struct node *children;
	STAILQ_ENTRY(node) link;
	struct proc *allocated_to;
	struct proc *provisioned_to;
};

STAILQ_HEAD(core_list, node);
struct proc {
	struct core_list core_owned;
	struct core_list core_provisioned;
};

void nodes_init();
void alloc_core_any(struct proc *p, int amt);
void alloc_core_specific(struct proc *p, int core_id);
int free_core_specific(struct proc *p, int core_id);
void provision_core(struct proc *p, int core_id);
void deprovision_core(struct proc *p, int core_id);

void print_node(struct node *n);
void print_nodes(int type);
void print_all_nodes();
void test_structure();

#endif
