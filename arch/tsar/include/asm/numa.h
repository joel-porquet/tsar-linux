#ifndef _ASM_TSAR_NUMA_H
#define _ASM_TSAR_NUMA_H

#ifdef CONFIG_NUMA

#include <linux/cpumask.h>
#include <linux/numa.h>

extern unsigned char tsar_ywidth;
extern unsigned char tsar_xwidth;

/*
 * 40-bit physical addresses are structured as:
 * |39-36|35-32| 31-0          |
 * |  X  |  Y  | cluster local |
 */
#define PA_TO_CLUSTERID_X(pa)	(((pa) >> 36) & 0xF)
#define PA_TO_CLUSTERID_Y(pa)	(((pa) >> 32) & 0xF)
#define PA_TO_LOCAL_ADDR(pa)	((pa) & 0xFFFFFFFF)

/*
 * 10-bit hw cpuid are structured as:
 * |9-6|5-2|1-0       |
 * | X | Y | local id |
 */
#define HWCPUID_TO_CLUSTERID_X(cpu)	(((cpu) >> 6) & 0xF)
#define HWCPUID_TO_CLUSTERID_Y(cpu)	(((cpu) >> 2) & 0xF)
#define HWCPUID_TO_LOCAL_ID(cpu)	((cpu) & 0x3)

int paddr_to_nid(phys_addr_t paddr);

/* in mm/numa.c */
void __init memory_setup_nodes(void);

/* in kernel/numa.c */
void __init cpu_setup_nodes(void);

/*
 * from cpu to node and vice-versa
 */
extern unsigned char cpu_node_map[NR_CPUS];
extern cpumask_t node_cpumask_map[MAX_NUMNODES];

/*
 * print formatting
 */
#define CPU_FMT_STR "CPU_%ld_%ld_%ld"
#define CPU_FMT_ARG(n)		\
	HWCPUID_TO_CLUSTERID_X(n),	\
	HWCPUID_TO_CLUSTERID_Y(n),	\
	HWCPUID_TO_LOCAL_ID(n)

#else

#define HWCPUID_TO_LOCAL_ID(cpu)	(cpu)
#define paddr_to_nid(paddr)	(0)

#define CPU_FMT_STR "CPU_%ld"
#define CPU_FMT_ARG(n) (n)

#endif

#endif /* _ASM_TSAR_NUMA_H */
