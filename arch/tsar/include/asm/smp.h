#ifndef _ASM_TSAR_SMP_H
#define _ASM_TSAR_SMP_H

#include <linux/thread_info.h>

#include <asm/ptrace.h>

#ifndef CONFIG_SMP
# error "<asm/smp.h> should not included in non-SMP build"
#endif


/*
 * SMP macros
 */

#define raw_smp_processor_id() (current_thread_info()->cpu)


/*
 * Logical to physical CPU mapping
 */

extern unsigned long __cpu_logical_map[NR_CPUS];
#define cpu_logical_map(cpu) __cpu_logical_map[cpu]


/*
 * IPI functions
 */

extern void handle_IPI(unsigned int);

#endif /* _ASM_TSAR_SMP_H */
