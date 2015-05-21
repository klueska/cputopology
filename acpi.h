/*
 * Copyright (c) 20015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * See LICENSE for details.
 */

#ifndef ACPI_H_
#define ACPI_H_

struct Apicst {
	int type;
	struct {
		int id;
	} lapic;
	struct Apicst *next;
};
struct Madt {
	struct Apicst *st;
};
#define ASlapic 0
extern struct Madt *apics;

void acpiinit();

#endif /* !ACPI_H */
