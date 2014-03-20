/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/ftrace.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/irqdomain.h>
#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>

#include <asm/io.h>

/*
 * Driver for the SoCLib VCI Interrupt Controller Unit (vci_icu)
 */

/* up to 32 hw IRQs */
#define VCI_ICU_IRQ_COUNT	32

/* vci_icu register map */
#define VCI_ICU_INT 		0x00 // RO: current state of IRQs
#define VCI_ICU_MASK 		0x04 // RO: mask of enabled IRQs
#define VCI_ICU_MASK_SET	0x08 // WO: set IRQ mask
#define VCI_ICU_MASK_CLEAR	0x0c // WO: unset IRQ mask
#define VCI_ICU_IT_VECTOR	0x10 // RO: highest active IRQ

static void __iomem *vci_icu_virt_base;
static struct irq_domain *vci_icu_irq_domain;

static inline void vci_icu_mask(struct irq_data *d)
{
	/* mask the requested irq (hwirq is the hardware irq number, local to
	 * the vci_icu) */
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	__raw_writel(BIT(hwirq), vci_icu_virt_base + VCI_ICU_MASK_CLEAR);
}

static inline void vci_icu_unmask(struct irq_data *d)
{
	/* unmask the requested irq (hwirq is the hardware irq number, local to
	 * the vci_icu) */
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	__raw_writel(BIT(hwirq), vci_icu_virt_base + VCI_ICU_MASK_SET);
}

static struct irq_chip vci_icu_controller = {
	.name		= "vci_icu",
	.irq_mask 	= vci_icu_mask,
	.irq_mask_ack 	= vci_icu_mask,
	.irq_unmask 	= vci_icu_unmask,
};

static asmlinkage __irq_entry
HANDLE_IRQ(vci_icu_handle_irq)
{
	s32 hwirq;
	unsigned int virq;

	/* get the index of the highest-priority active hwirq */
	hwirq = __raw_readl(vci_icu_virt_base + VCI_ICU_IT_VECTOR);

	if (hwirq >= 0 && hwirq < VCI_ICU_IRQ_COUNT) {
		/* find the corresponding virq */
		virq = irq_find_mapping(vci_icu_irq_domain, hwirq);
		/* and call the main IRQ handler */
		handle_IRQ(virq, regs);
	} else {
		ack_bad_irq(hwirq);
	}
}

static int vci_icu_map(struct irq_domain *d, unsigned int virq,
		irq_hw_number_t hw)
{
	/* associate irqs with vci_icu controller */
	/* IRQs on vci_icu are level type interrupts */
	irq_set_chip_and_handler(virq, &vci_icu_controller, handle_level_irq);
	irq_set_status_flags(virq, IRQ_LEVEL);
	return 0;
}

static const struct irq_domain_ops vci_icu_domain_ops = {
	.map	= vci_icu_map,
	.xlate	= irq_domain_xlate_onecell,
};

int __init vci_icu_init(struct device_node *of_node, struct device_node *parent)
{
	struct resource res;

	BUG_ON(of_address_to_resource(of_node, 0, &res));
	BUG_ON(!request_mem_region(res.start, resource_size(&res), res.name));

	vci_icu_virt_base = ioremap_nocache(res.start, resource_size(&res));

	BUG_ON(!vci_icu_virt_base);

	/* add an irq domain for the vci_icu */
	vci_icu_irq_domain = irq_domain_add_linear(of_node, VCI_ICU_IRQ_COUNT,
			&vci_icu_domain_ops, NULL);
	BUG_ON(!vci_icu_irq_domain);

	/* disable all vci_icu IRQs */
	__raw_writel(~0, vci_icu_virt_base + VCI_ICU_MASK_CLEAR);

	/* provide an IRQ handler for the vcu_icu */
	set_handle_irq(vci_icu_handle_irq);

	return 0;
}

IRQCHIP_DECLARE(vci_icu, "soclib,vci_icu", vci_icu_init);
