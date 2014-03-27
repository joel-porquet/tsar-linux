#ifndef _ASM_TSAR_SMP_MAP_H
#define _ASM_TSAR_SMP_MAP_H

/*
 * Logical to physical CPU mapping
 */

#define INVALID_HWID	ULONG_MAX

extern unsigned long __cpu_logical_map[NR_CPUS];
#define cpu_logical_map(cpu) __cpu_logical_map[cpu]

#endif /* _ASM_TSAR_SMP_MAP_H */
