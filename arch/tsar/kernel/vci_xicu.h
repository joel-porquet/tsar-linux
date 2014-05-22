#ifndef _TSAR_VCI_XICU_H
#define _TSAR_VCI_XICU_H

#include <linux/compiler.h>
#include <linux/irqdomain.h>

/*
 * SoCLib VCI Generic Interrupt Controller Unit (soclib,vci_xicu)
 *
 * This component provides:
 * - 32 external hw irqs (HWI)
 * - 32 internal soft irqs (IPI)
 * - 32 internal timers (PTI)
 * - 32 outputs
 *
 * Here we slightly limit this flexibility:
 * - in regular systems, there is one output dedicated per cpu: cpu[n] <=
 *   output[n]. In LETI system, there is four dedicated outputs per cpu: cpu[n]
 *   <= output[n * 4] to output [(n * 4) + 3]. But we only use the first one.
 * - the 32 external HWI are shared, in the sense that they can migrate from
 *   one cpu (usually the boot cpu at bootime) to another at runtime.
 * - we use one internal IPI and one internal PTI per cpu. From cpu view and
 *   within the xicu irq domain, the IPI is always irq #0, while the PTI is
 *   always irq #1. The numbering of the HWI starts at 2.
 *   E.g.:
 *   cpu[n] <= output[N] <= irq[0]   <= ipi[n]
 *   cpu[n] <= output[N] <= irq[1]   <= pti[n]
 *   cpu[n] <= output[N] <= irq[2+m] <= hwi[m] (relevant only if hwi[m] is
 *   unmasked for cpu[n])
 *
 *      Note: N is n for regular systems, but is (n * 4) for LETI system.
 */

/* IPI IRQ is always #0 */
#define VCI_XICU_IPI_PER_CPU_IRQ	0
/* PTI IRQ is always #1 */
#define VCI_XICU_PTI_PER_CPU_IRQ	1
/* hw IRQs start at #2 */
#define VCI_XICU_MAX_PER_CPU_IRQ	2

/* The 4Kb page of the XICU is partitioned into 32-bit configuration registers,
 * which is determined by the combination of functions and indexes (32 at most)
 * FUNC (5 bits) | INDEX (5 bits) | 00
 */
#define VCI_XICU_REG(func, index) \
	((unsigned long *)vci_xicu_virt_base + \
	(((((func) & 0x1f) << 5) | ((index) & 0x1f)) & 0x3ff))

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

extern void __iomem *vci_xicu_virt_base;
extern struct irq_domain *vci_xicu_irq_domain;

/* Convert hardware cpu number into xicu irq output */
#ifdef CONFIG_SOCLIB_VCI_XICU_LETI
/* on LETI hardware system, there are 4 irq outputs per cpu */
# define VCI_XICU_CPUID_MAP(x) ((x) << 2)
#else
/* on regular hardware system, there is 1 irq output per cpu */
# define VCI_XICU_CPUID_MAP(x) (x)
#endif

#endif /* _TSAR_VCI_XICU_H */
