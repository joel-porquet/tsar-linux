/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */
#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/smp.h>

#include <asm/io.h>
#include <asm/smp_map.h>

#include "vci_xicu.h"

/*
 * Driver for the SoCLib VCI IOPIC (soclib,vci_iopic)
 *
 * The iopic is a interrupt controller. It is connected to up to 32 interrupt
 * sources from peripheral devices, and can redirect those interrupts to any
 * XICUs of the system using WTIs.
 */

/* up to 32 external hw IRQs */
#define VCI_IOPIC_IRQ_COUNT	32

/* vci_ioc registers map */
#define IOPIC_ADDRESS	0x0
#define IOPIC_EXTEND	0x1
#define IOPIC_STATUS	0x2
#define IOPIC_MASK	0x3

#define VCI_IOPIC_REG(index, func)	\
	((unsigned long *)(iopic.virt +	\
		(((index & 0x1f) << 4) | ((func & 0x3) << 2))))

struct vci_iopic {
	raw_spinlock_t		lock;
	void __iomem		*virt;		/* mapped address */
	struct irq_domain	*irq_domain;	/* associated irq domain */

	/* association between an IRQ source and a cpu (logical number) */
	int			irq_to_cpu[VCI_IOPIC_IRQ_COUNT];
};

struct vci_iopic iopic;

/*
 * Irqchip driver for IRQ of IOPIC
 *
 * IOPIC IRQs are directly mapped on XICU WTIs, with an offset of
 * MAX_CPU_PER_CLUSTER (for IPIs). That is why we cannot mapped the 32 IRQs
 * (but 28 on a 4 processors/cluster system, which seems perfectly enough).
 */

static inline void __vci_iopic_generic_mask_enable(struct irq_data *d,
		bool mask, bool do_enable, bool enable)
{
	struct vci_xicu *xicu;
	irq_hw_number_t hwirq;
	unsigned long cpu, hw_cpu, node_hw_cpu, outirq;
	unsigned char cmd;

	union {
		phys_addr_t paddr;
		struct {
			unsigned long low;
			unsigned long high;
		};
	} xicu_iopic_addr;

	raw_spin_lock(&iopic.lock);

	hwirq = irqd_to_hwirq(d);
	cpu = iopic.irq_to_cpu[hwirq];
	xicu = vci_xicu[cpu_to_node(cpu)];
	compute_hwcpuid(cpu, &hw_cpu, &node_hw_cpu);

	BUG_ON(!xicu);
	BUG_ON(hwirq >= MAX_WTI_COUNT - MAX_CPU_PER_CLUSTER);
	BUG_ON(hw_cpu == INVALID_HWCPUID);
	BUG_ON(node_hw_cpu > MAX_CPU_PER_CLUSTER);

	outirq = VCI_XICU_CPUID_MAP(node_hw_cpu);

	pr_debug("Node%d: (un)mask IOIRQ %ld on CPU%ld\n",
			xicu->node, hwirq, hw_cpu);

	cmd = mask ? XICU_MSK_WTI_DISABLE : XICU_MSK_WTI_ENABLE;

	writel(BIT(hwirq + MAX_CPU_PER_CLUSTER),
			VCI_XICU_REG(xicu, cmd, outirq));

	if (do_enable) {
		pr_debug("enable/disable IOIRQ %ld\n", hwirq);

		if (enable) {
			/* configure the redirection address and unmask when
			 * enabling */
			xicu_iopic_addr.paddr =
				VCI_XICU_REG_PADDR(xicu, cmd, outirq);
			writel(xicu_iopic_addr.low,
					VCI_IOPIC_REG(hwirq, IOPIC_ADDRESS));
			writel(xicu_iopic_addr.high,
					VCI_IOPIC_REG(hwirq, IOPIC_EXTEND));
			writel(1, VCI_IOPIC_REG(hwirq, IOPIC_MASK));
		} else {
			/* only mask when disabling */
			writel(0, VCI_IOPIC_REG(hwirq, IOPIC_MASK));
		}
	}

	raw_spin_unlock(&iopic.lock);
}

static void vci_iopic_mask(struct irq_data *d)
{
	__vci_iopic_generic_mask_enable(d, true, false, false);
}

static void vci_iopic_unmask(struct irq_data *d)
{
	__vci_iopic_generic_mask_enable(d, false, false, false);
}

static void vci_iopic_enable(struct irq_data *d)
{
	__vci_iopic_generic_mask_enable(d, true, true, true);
}

static void vci_iopic_disable(struct irq_data *d)
{
	__vci_iopic_generic_mask_enable(d, false, true, true);
}

static int vci_iopic_set_affinity(struct irq_data *d,
		const cpumask_t *cpumask_req, bool force)
{
	irq_hw_number_t hwirq;
	unsigned long chosen_cpu;

	hwirq = irqd_to_hwirq(d);

	chosen_cpu = cpumask_any_and(cpumask_req, cpu_online_mask);

	if (chosen_cpu >= nr_cpu_ids)
		return -EINVAL;

	pr_debug("Migrate IOIRQ %ld to CPU%ld\n",
			hwirq, cpu_logical_map(chosen_cpu));

	/* disable the old association */
	vci_iopic_disable(d);

	/* update the association between the IOIRQ and the newly chosen cpu */
	raw_spin_lock(&iopic.lock);
	iopic.irq_to_cpu[hwirq] = chosen_cpu;
	raw_spin_unlock(&iopic.lock);

	/* enable the new association */
	vci_iopic_enable(d);

	return IRQ_SET_MASK_OK;
}

static struct irq_chip vci_iopic_controller = {
	.name		= "vci_iopic",
	.irq_enable	= vci_iopic_enable,
	.irq_disable	= vci_iopic_disable,
	.irq_mask	= vci_iopic_mask,
	.irq_mask_ack	= vci_iopic_mask,
	.irq_unmask	= vci_iopic_unmask,
	.irq_set_affinity = vci_iopic_set_affinity,
};

/*
 * Irq domain driver
 */

static int vci_iopic_map(struct irq_domain *d, unsigned int virq,
		irq_hw_number_t hwirq)
{
	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &vci_iopic_controller,
			handle_level_irq);
	return 0;
}

static int vci_iopic_xlate(struct irq_domain *d, struct device_node *ctrlr,
		const u32 *intspec, unsigned int intsize,
		unsigned long *out_hwirq, unsigned int *out_type)
{
	int ret;
	ret = irq_domain_xlate_onecell(d, ctrlr, intspec, intsize,
			out_hwirq, out_type);

	/* check the IRQ number fits the available number of WTI in XICUs */
	if (!ret && (WARN_ON(*out_hwirq >= MAX_WTI_COUNT -
					MAX_CPU_PER_CLUSTER)))
		return -EINVAL;
	return ret;
}

static const struct irq_domain_ops vci_iopic_domain_ops = {
	.map	= vci_iopic_map,
	.xlate	= vci_iopic_xlate,
};

/*
 * VCI_IOPIC ISR
 */
void vci_iopic_handle_irq(irq_hw_number_t wti, struct pt_regs *regs)
{
	unsigned int virq;

	virq = irq_find_mapping(iopic.irq_domain, wti);
	handle_IRQ(virq, regs);
}

void __init vci_iopic_state_init(void)
{
	size_t i;

	/* deactivate all IRQs and initialize irq_to_cpu association */
	for (i = 0; i < VCI_IOPIC_IRQ_COUNT; i++) {
		iopic.irq_to_cpu[i] = smp_processor_id();
		writel(0, VCI_IOPIC_REG(i, IOPIC_MASK));
	}
}

int __init vci_iopic_init(struct device_node *of_node, struct device_node *parent)
{
	struct resource res;

	BUG_ON(of_address_to_resource(of_node, 0, &res));
	BUG_ON(!request_mem_region(res.start, resource_size(&res), res.name));

	iopic.virt = ioremap_nocache(res.start, resource_size(&res));
	BUG_ON(!iopic.virt);

	iopic.irq_domain = irq_domain_add_linear(of_node,
			32, &vci_iopic_domain_ops, NULL);
	BUG_ON(!iopic.irq_domain);

	raw_spin_lock_init(&iopic.lock);

	vci_iopic_state_init();

	return 0;
}

IRQCHIP_DECLARE(vci_iopic, "soclib,vci_iopic", vci_iopic_init);
