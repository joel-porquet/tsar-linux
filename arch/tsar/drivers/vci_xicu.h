#ifndef _TSAR_VCI_XICU_H
#define _TSAR_VCI_XICU_H

#include <linux/compiler.h>
#include <linux/irqdomain.h>

#include <asm/numa.h>

/*
 * SoCLib VCI Generic Interrupt Controller Unit (soclib,vci_xicu)
 *
 * This component provides:
 * - 32 external hw irqs (HWI)
 * - 32 internal soft irqs (WTI)
 * - 32 internal timers (PTI)
 * - 32 outputs (IRQ)
 *
 * The XICU is fully flexible and enables any combination possible, but here we
 * limit this flexibility for our needs:
 *
 * - We consider that there is a fixed number of IRQ output lines
 *   (2^CONFIG_SOCLIB_VCI_XICU_CPUIRQS_SHIFT) connecting a xicu to each cpu of
 *   the same cluster.
 *
 *   - In most systems, there is one IRQ per cpu (ie
 *   CONFIG_SOCLIB_VCI_XICU_CPUIRQS_SHIFT = 0) such as cpu[n] <= output[n].
 *
 *   - In other systems, such a LETI, there is four dedicated IRQs per cpu (ie
 *   CONFIG_SOCLIB_VCI_XICU_CPUIRQS_SHIFT = 2) such as cpu[n] <=
 *   outputs[(n*4):(n*4)+3]. But we always only use the first IRQ of a certain
 *   range.
 *
 * - If the external HWIs are activated (see CONFIG_SOCLIB_VCI_XICU_HWI token),
 *   they can migrate from one cpu to another one in the same cluster at
 *   runtime.
 *
 * - In this driver, we use a fixed number of internal interrupts per cpu:
 *   - 1 internal IPI (for inter cpu communication when SMP),
 *   - 1 internal PTI (percpu timer).
 *   The numbering of the HWI starts after this fixed number of internal
 *   interrupts, with respect to the interrupt numbering in a irq domain.
 *
 * - When the IOPIC is enabled, the available WTIs are used as an extension of
 *   HWIs.
 */

#ifdef CONFIG_TSAR_MULTI_CLUSTER
/* 4 cpus per cluster when doing multi-cluster*/
# define MAX_CPU_PER_CLUSTER 4
#else
/* otherwise no limit (actually 32) */
# define MAX_CPU_PER_CLUSTER num_possible_cpus()
#endif

/* 32 possible sources of IRQ of each kind */
#define MAX_WTI_COUNT 32
#define MAX_PTI_COUNT 32
#define MAX_HWI_COUNT 32

enum hwirq_map {
	/* percpu IRQs */
#ifdef CONFIG_SMP
	IPI_IRQ,
#endif
	PTI_IRQ,

	/* global hardware IRQs */
	MAX_PCPU_IRQS,
	HWIRQ_START = MAX_PCPU_IRQS,
};

/* The 4Kb page of the XICU is partitioned into 32-bit configuration registers,
 * which is determined by the combination of functions and indexes (32 at most)
 * FUNC (5 bits) | INDEX (5 bits) | 00
 */
#define __VCI_XICU_REG_OFFSET(func, index) \
	 (((((func) & 0x1f) << 5) | ((index) & 0x1f)) & 0x3ff)

#define VCI_XICU_REG(xicu, func, index) \
	((unsigned long *)((xicu)->virt) + __VCI_XICU_REG_OFFSET(func, index))

#define VCI_XICU_REG_PADDR(xicu, func, index) \
	((phys_addr_t)((xicu)->paddr) + (__VCI_XICU_REG_OFFSET(func, index) << 2))

/* VCI_XICU functions */
#define XICU_WTI_REG		0x0 	/* indexed by WTI_INDEX	(R/W) */
#define XICU_PTI_PER		0x1 	/* indexed by PTI_INDEX	(R/W) */
#define XICU_PTI_VAL		0x2 	/* indexed by PTI_INDEX	(R/W) */
#define XICU_PTI_ACK		0x3 	/* indexed by PTI_INDEX	(W) */
#define XICU_MSK_PTI		0x4	/* indexed by 0UT_INDEX	(R/W) */
#define XICU_MSK_PTI_ENABLE	0x5 	/* indexed by OUT_INDEX	(W) */
#define XICU_MSK_PTI_DISABLE	0x6 	/* indexed by OUT_INDEX	(W) */
#define XICU_PTI_ACTIVE		0x6 	/* indexed by OUT_INDEX	(R) */
//#define Reserved 	0x7
#define XICU_MSK_HWI		0x8 	/* indexed by OUT_INDEX	(R/W) */
#define XICU_MSK_HWI_ENABLE	0x9 	/* indexed by OUT_INDEX	(W) */
#define XICU_MSK_HWI_DISABLE	0xa 	/* indexed by OUT_INDEX	(W) */
#define XICU_HWI_ACTIVE		0xa 	/* indexed by OUT_INDEX	(R) */
//#define Reserved 	0xb
#define XICU_MSK_WTI		0xc 	/* indexed by OUT_INDEX	(R/W) */
#define XICU_MSK_WTI_ENABLE	0xd 	/* indexed by OUT_INDEX	(W) */
#define XICU_MSK_WTI_DISABLE	0xe 	/* indexed by OUT_INDEX	(W) */
#define XICU_WTI_ACTIVE		0xe 	/* indexed by OUT_INDEX	(R) */
#define XICU_PRIO		0xf 	/* indexed by OUT_INDEX (R) */
#define XICU_CONFIG		0x10 	/* not indexed (R) */

/* Priority encoder, one per OUT_INDEX
 * 000|PRIO_WTI (5bits)|000|PRIO_HWI (5bits)|000|PRIO_PTI (5bits)|00000|W|H|T */
#define XICU_PRIO_HAS_PTI(prio) ((prio) & 0x1)
#define XICU_PRIO_HAS_HWI(prio) ((prio) & 0x2)
#define XICU_PRIO_HAS_WTI(prio) ((prio) & 0x4)

#define XICU_PRIO_PTI(prio) (((prio) >> 8)  & 0x1f)
#define XICU_PRIO_HWI(prio) (((prio) >> 16) & 0x1f)
#define XICU_PRIO_WTI(prio) (((prio) >> 24) & 0x1f)

/* Config register
 * 00|IRQ_COUNT|00|WTI_COUNT|00|HWI_COUNT|00|PTI_COUNT */
#define XICU_CONFIG_IRQ_COUNT(conf) (((conf) >> 24) & 0x3f)
#define XICU_CONFIG_WTI_COUNT(conf) (((conf) >> 16) & 0x3f)
#define XICU_CONFIG_HWI_COUNT(conf) (((conf) >> 8)  & 0x3f)
#define XICU_CONFIG_PTI_COUNT(conf) (((conf) >> 0)  & 0x3f)

/* Map a cpu number to the first IRQ output connected to it. On some systems,
 * this is not an identity mapping (CPU0 connected to IRQ0) but there can be
 * ranges of 2^CONFIG_SOCLIB_VCI_XICU_CPUIRQS_SHIFT IRQs connected to each cpu
 */
#define VCI_XICU_CPUID_MAP(x) ((x) << CONFIG_SOCLIB_VCI_XICU_CPUIRQS_SHIFT)

/*
 * Description of a XICU
 */
struct vci_xicu {
	raw_spinlock_t	lock;	/* lock for protecting access to xicu
				   (especially for the shared HW IRQs).  Use of
				   a raw spinlock because we don't want to go
				   to sleep when we can't get it */

	phys_addr_t	paddr;	/* physical address */
	void __iomem	*virt;	/* mapped address after ioremap() */

	int		node;		/* numa node */

	struct irq_domain	*irq_domain;	/* associated irq domain */

	/* IPI */
	unsigned int	ipi_irq;	/* IRQ number for IPI */

	/* HWI */
#ifdef CONFIG_SOCLIB_VCI_XICU_HWI
	/* association between a hwi and a cluster-local cpu */
	unsigned char	hwi_to_hw_node_cpu[MAX_HWI_COUNT];
#endif

	/* PTI */
	unsigned long	clk_period;	/* clock period (cycles) */
	unsigned long	clk_rate;	/* clock frequency */
	unsigned int	timer_irq;	/* IRQ number for PTI */

	/* properties */
	size_t		irq_count;	/* number of output IRQs */
	size_t		hwi_count;	/* number of HWI IRQs */
	size_t		wti_count;	/* number of WTI IRQs */
	size_t		pti_count;	/* number of PTI IRQs */
};

extern struct vci_xicu *vci_xicu[MAX_NUMNODES];

/*
 * from the logical cpu id of a processor, get its hardware cpu id (in
 * cpu_logical_map[] lookup table) and deduce its cluster-local id
 */
static inline void compute_hwcpuid(unsigned int logical_cpu,
		unsigned long *hw_cpu, unsigned long *node_hw_cpu)
{
	*hw_cpu = cpu_logical_map(logical_cpu);
	BUG_ON(*hw_cpu == INVALID_HWCPUID);
	*node_hw_cpu = HWCPUID_TO_LOCAL_ID(*hw_cpu);
	BUG_ON(*node_hw_cpu >= MAX_CPU_PER_CLUSTER);
}

extern void vci_iopic_handle_irq(irq_hw_number_t wti, struct pt_regs *regs);

#endif /* _TSAR_VCI_XICU_H */
