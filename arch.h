/*
 * Copyright (c) 20015 The Regents of the University of California
 * Valmon Leymarie <leymariv@berkeley.edu>
 * See LICENSE for details.
 */

#ifndef ARCH_H_
#define ARCH_H_

void archinit();
void pin_to_core(int coreid);
uint32_t get_apic_id();

static inline void cpuid(uint32_t info1, uint32_t info2, uint32_t *eaxp,
                         uint32_t *ebxp, uint32_t *ecxp, uint32_t *edxp)
{
	uint32_t eax, ebx, ecx, edx;
	/* Can select with both eax (info1) and ecx (info2) */
	asm volatile("cpuid" 
				 : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
				 : "a" (info1), "c" (info2));
	if (eaxp)
		*eaxp = eax;
	if (ebxp)
		*ebxp = ebx;
	if (ecxp)
		*ecxp = ecx;
	if (edxp)
		*edxp = edx;
}

#endif /* !ARCH_H */
