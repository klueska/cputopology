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
#include <pthread.h>
#include <stdint.h>

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

static void pin_to_core(int i)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(i, &cpuset);
  sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
  sched_yield();
}

static void *core_proxy(void *arg)
{
  /* Pin to core */
  int coreid = (int)(long)arg;
  pin_to_core(coreid);

  /* Do some cpuid stuff */
  uint32_t eax, ebx, ecx, edx;
  uint32_t x2apic_id, core_plus_mask_width,
    core_only_select_mask,core_id;

 
  uint32_t apic_id,count,logical_cpu_bits,core_bits,logical_CPU_number_within_core,core_number_within_chip,chip_id;

  eax = 0x00000001;
  ecx = 0;
  cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);

  apic_id = ebx >>24;


  eax = 0x0000000b;
  ecx = 0;
  cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
  logical_cpu_bits = eax;

  eax = 0x0000000b;
  ecx = 1;
  cpuid(eax, ecx, &eax, &ebx, &ecx, &edx);
  core_bits = eax - logical_cpu_bits;
  
  logical_CPU_number_within_core = apic_id & ( (1 << logical_cpu_bits) -1) ;
  core_number_within_chip = (apic_id >> logical_cpu_bits) & ( (1 << core_bits) -1) ;
  chip_id = apic_id & ~( (1 << (logical_cpu_bits+core_bits) ) -1) ;

  printf("I am core: %d, logical_CPU_number_within_core: 0x%08x,apic_id: 0x%08x,core_number_within_chip: 0x%08x, chip_id: 0x%08x\n",
  	 coreid, logical_CPU_number_within_core, apic_id, core_number_within_chip,chip_id);

}

int main(int argc, char **argv)
{
  int ncpus = get_nprocs();
  pthread_t pthread[ncpus];
  for (int i=0; i<ncpus; i++) {
    pthread_create(&pthread[i], NULL, core_proxy, (void*)(long)i);
  }
  for (int i=0; i<ncpus; i++) {
    pthread_join(pthread[i], NULL);
  }
	
  return 0;
}

