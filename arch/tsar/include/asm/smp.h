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

#define INVALID_HWID	ULONG_MAX

extern unsigned long __cpu_logical_map[NR_CPUS];
#define cpu_logical_map(cpu) __cpu_logical_map[cpu]


/*
 * Initial data for booting a secondary cpu
 */

struct secondary_data {
	unsigned long sp;	/* stack pointer */
	unsigned long gp;	/* global pointer */
};

/*
 * IPI management
 */

extern void handle_IPI(void);

#define SMP_IPI_CALL(n) void (n)(const struct cpumask *mask)
typedef SMP_IPI_CALL(smp_ipi_call_t);

extern void set_smp_ipi_call(smp_ipi_call_t *);

#endif /* _ASM_TSAR_SMP_H */
