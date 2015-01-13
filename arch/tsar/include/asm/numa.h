#ifndef _ASM_TSAR_NUMA_H
#define _ASM_TSAR_NUMA_H

#ifdef CONFIG_NUMA

#include <linux/cpumask.h>
#include <linux/numa.h>

/* node grid size */
extern unsigned char tsar_ywidth;
extern unsigned char tsar_xwidth;

/* lowmem mapping */
extern unsigned char node_lowmem_sz_log2;
extern unsigned char node_lowmem_sc_log2;

extern unsigned long node_lowmem_sz_mask;
extern unsigned char node_lowmem_sc_mask;

#define NUMA_HIGHMEM_NODE(nid) !!((nid) & node_lowmem_sc_mask)

#define NID_TO_LOWMEM_VADDR(nid)	\
	__va_offset(((nid) >> node_lowmem_sc_log2) << node_lowmem_sz_log2)
#define LOWMEM_VADDR_TO_NID(vaddr)	\
	((__pa_offset(vaddr) >> node_lowmem_sz_log2) << node_lowmem_sc_log2)

/*
 * 40-bit physical addresses are structured as:
 * |39-36|35-32| 31-0                  |
 * |  X  |  Y  | cluster local address |
 */
#define CLUSTERID_MASK		0xfU
#define CLUSTERLA_MASK		0xffffffffUL

#define PA_TO_CLUSTERID_X(pa)	(((pa) >> 36) & CLUSTERID_MASK)
#define PA_TO_CLUSTERID_Y(pa)	(((pa) >> 32) & CLUSTERID_MASK)

#define PA_TO_LOCAL_ADDR(pa)	((pa) & CLUSTERLA_MASK)
#define PFN_TO_LOCAL_PFN(pfn)	((pfn) & PFN_DOWN(CLUSTERLA_MASK))

/*
 * 10-bit hw cpuid are structured as:
 * |9-6|5-2|1-0               |
 * | X | Y | cluster local id |
 */
#define CLUSTERLI_MASK			0x3U

#define HWCPUID_TO_CLUSTERID_X(cpu)	(((cpu) >> 6) & CLUSTERID_MASK)
#define HWCPUID_TO_CLUSTERID_Y(cpu)	(((cpu) >> 2) & CLUSTERID_MASK)
#define HWCPUID_TO_LOCAL_ID(cpu)	((cpu) & CLUSTERLI_MASK)

static inline unsigned int paddr_to_nid(phys_addr_t paddr)
{
	unsigned char x = PA_TO_CLUSTERID_X(paddr);
	unsigned char y = PA_TO_CLUSTERID_Y(paddr);

	unsigned char nid = y * tsar_xwidth + x;

	/* don't check the y coordinate to allow the IO node on LETI system
	 * to get a node number from this function */
	BUG_ON(x >= tsar_xwidth);

	return nid;
}

static inline phys_addr_t nid_to_paddr(unsigned int nid)
{
	unsigned char x = nid % tsar_xwidth;
	unsigned char y = nid / tsar_xwidth;

	return ((phys_addr_t)x << 36 | (phys_addr_t)y << 32);
}


/* in mm/numa.c */
void *__init early_memory_setup_nodes(unsigned long);
void __init memory_setup_nodes(void);

/* in kernel/numa.c */
void __init cpu_setup_nodes(void);

/*
 * from cpu to node and vice-versa
 */
extern unsigned char cpu_node_map[NR_CPUS];
extern cpumask_t node_cpumask_map[MAX_NUMNODES];

#ifdef CONFIG_KTEXT_REPLICATION
/*
 * ktext replication
 */
extern unsigned char node_ktext_count;
extern unsigned char node_ktext_sc_log2;

void numa_ktext_patch_pgd(pgd_t *pgd, unsigned int nid);
pgd_t *numa_ktext_get_pgd(unsigned int nid);

#define NID_TO_KTEXT_NID(nid)	(nid >> node_ktext_sc_log2)
#define KTEXT_NID_TO_NID(knid)	(knid << node_ktext_sc_log2)
#else
#define NID_TO_KTEXT_NID(nid)	(0)
#endif

/*
 * print formatting
 */
#define CPU_FMT_STR "CPU_%ld_%ld_%ld"
#define CPU_FMT_ARG(n)		\
	HWCPUID_TO_CLUSTERID_X(n),	\
	HWCPUID_TO_CLUSTERID_Y(n),	\
	HWCPUID_TO_LOCAL_ID(n)

#else

#define PA_TO_LOCAL_ADDR(pa)		(pa)
#define PFN_TO_LOCAL_PFN(pfn)		(pfn)

#define NUMA_HIGHMEM_NODE(nid)		(0)

#define HWCPUID_TO_LOCAL_ID(cpu)	(cpu)
#define paddr_to_nid(paddr)		(0)

#define CPU_FMT_STR "CPU_%ld"
#define CPU_FMT_ARG(n) (n)

#endif

#endif /* _ASM_TSAR_NUMA_H */
