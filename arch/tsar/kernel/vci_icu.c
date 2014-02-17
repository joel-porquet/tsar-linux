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

static inline void vci_icu_mask(struct irq_data *d)
{
	/* mask the requested irq (hwirq is the hardware irq number, local to
	 * the vci_icu) */
	__raw_writel(BIT(d->hwirq), vci_icu_virt_base + VCI_ICU_MASK_CLEAR);
}

static inline void vci_icu_unmask(struct irq_data *d)
{
	/* unmask the requested irq (hwirq is the hardware irq number, local to
	 * the vci_icu) */
	__raw_writel(BIT(d->hwirq), vci_icu_virt_base + VCI_ICU_MASK_SET);
}

static struct irq_chip vci_icu_controller = {
	.name		= "vci_icu",
	.irq_mask 	= vci_icu_mask,
	.irq_mask_ack 	= vci_icu_mask,
	.irq_unmask 	= vci_icu_unmask,
};

static void vci_icu_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	/* get the index of the highest-priority active irq */
	s32 pending_irq = __raw_readl(vci_icu_virt_base + VCI_ICU_IT_VECTOR);

	if (pending_irq >= 0 && pending_irq < NR_IRQS) {
		/* find the domain for vci_icu */
		struct irq_domain *domain = irq_get_handler_data(irq);
		generic_handle_irq(irq_find_mapping(domain, pending_irq));
	} else {
		ack_bad_irq(pending_irq);
	}
}

static int vci_icu_map(struct irq_domain *d, unsigned int irq,
		irq_hw_number_t hw)
{
	/* associate irqs with vci_icu controller */
	/* IRQs on vci_icu are level type interrupts */
	irq_set_chip_and_handler(irq, &vci_icu_controller, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops vci_icu_domain_ops = {
	.map = vci_icu_map,
	.xlate = irq_domain_xlate_onecell,
};

int __init vci_icu_init(struct device_node *of_node, struct device_node *parent)
{
	struct irq_domain *domain;
	unsigned int irq;
	struct resource res;

	/* get virq of the irq that links the vci_icu to the parent icu (ie
	 * mips32_icu) */
	irq = irq_of_parse_and_map(of_node, 0);
	if (!irq)
		panic("%s: failed to get IRQ\n", of_node->full_name);

	if (of_address_to_resource(of_node, 0, &res))
		panic("%s: failed to get memory range\n", of_node->full_name);

	if (!request_mem_region(res.start, resource_size(&res), res.name))
		panic("%s: failed to request memory\n", of_node->full_name);

	vci_icu_virt_base = ioremap_nocache(res.start, resource_size(&res));

	if (!vci_icu_virt_base)
		panic("%s: failed to remap memory\n", of_node->full_name);

	/* add an irq domain for vci_icu */
	domain = irq_domain_add_linear(of_node, VCI_ICU_IRQ_COUNT,
			&vci_icu_domain_ops, NULL);
	if (!domain)
		panic("%s: failed to add irqdomain\n", of_node->full_name);

	/* disable all vci_icu IRQs */
	__raw_writel(~0, vci_icu_virt_base + VCI_ICU_MASK_CLEAR);

	/* link vci_icu to mips32_icu */
	irq_set_chained_handler(irq, vci_icu_irq_handler);
	irq_set_handler_data(irq, domain);

	return 0;
}
IRQCHIP_DECLARE(vci_icu, "soclib,vci_icu", vci_icu_init);
