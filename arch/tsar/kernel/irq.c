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
#include <linux/ratelimit.h>
#include <linux/seq_file.h>

#include <asm/io.h>
#include <asm/mips32c0.h>

/*
 * Stats about interrupts
 */

atomic_t irq_err_count;

void ack_bad_irq(unsigned int irq)
{
	printk_ratelimited(KERN_ERR "IRQ: spurious interrupt %d\n", (int)irq);
	atomic_inc(&irq_err_count);
}

int arch_show_interrupts(struct seq_file *p, int prec)
{
	seq_printf(p, "%*s: %10u\n", prec, "ERR", atomic_read(&irq_err_count));
	return 0;
}

/*
 * Driver for the mips32 Interrupt Controller Unit
 */

/* 2 soft IRQs and 6 hw IRQs */
#define MIPS32_ICU_IRQ_COUNT	8

/*
 * Low level ISR
 */

static inline int mips32_icu_get_irq(unsigned int first)
{
	int hwirq = 0;
	unsigned long r;
	unsigned int virq;

	/* get the state of IRQs:
	 * - status contains the mask
	 * - cause contains the active IRQ */
	r = (read_c0_status() & read_c0_cause()) >> 8;
	r &= 0xff;
 	r >>= first;

	if (r)
		hwirq = __ffs(r);
	if (!hwirq)
		return -1;
	else
		hwirq = hwirq + first;

	virq = irq_find_mapping(NULL, hwirq);
	return virq;
}

void __irq_entry do_IRQ(struct pt_regs *regs)
{
	int irq = -1;
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();

	/* while there is active IRQs, do them! */
	while ((irq = mips32_icu_get_irq(irq + 1)) != -1)
		generic_handle_irq(irq);

	irq_exit();
	set_irq_regs(old_regs);
}

/*
 * IRQ chip driver
 */

static inline void mips32_icu_mask(struct irq_data *d)
{
	/* mask the requested irq (hwirq is the hardware irq number, local to
	 * the mips32_icu) */
	clear_c0_status(0x100 << d->hwirq);
}

static inline void mips32_icu_unmask(struct irq_data *d)
{
	/* unmask the requested irq (hwirq is the hardware irq number, local to
	 * the mips32_icu) */
	set_c0_status(0x100 << d->hwirq);
}

static struct irq_chip mips32_icu_controller = {
	.name		= "mips32_icu",
	/* irq_mask, irq_mask_ack, irq_unmask seem to be the three mandatory
	 * callbacks to provide (actually irq_mask_ack doesn't seem to be 100%
	 * mandatory) */
	.irq_mask	= mips32_icu_mask,
	.irq_mask_ack	= mips32_icu_mask,
	.irq_unmask	= mips32_icu_unmask,
};

static int mips32_icu_map(struct irq_domain *d, unsigned int irq,
		irq_hw_number_t hw)
{
	/* associate irqs with mips32_icu controller */
	irq_set_chip_and_handler(irq, &mips32_icu_controller, handle_percpu_irq);
	return 0;
}

static const struct irq_domain_ops mips32_icu_domain_ops = {
	.map = mips32_icu_map,
	.xlate = irq_domain_xlate_onecell,
};

int __init mips32_icu_init(struct device_node *of_node, struct device_node *parent)
{
	struct irq_domain *domain;

	/* mask all the interrupts */
	clear_c0_status(ST0_IM);

	/* add an irq domain for mips32_icu */
	domain = irq_domain_add_linear(of_node, MIPS32_ICU_IRQ_COUNT,
			&mips32_icu_domain_ops, NULL);
	if (!domain)
		panic("%s: failed to add irqdomain\n", of_node->full_name);

	/* make it the default domain */
	irq_set_default_host(domain);

	return 0;
}
IRQCHIP_DECLARE(mips32_icu, "soclib,mips32_icu", mips32_icu_init);

/*
 * IRQ global initialization
 */

void __init init_IRQ(void)
{
	irqchip_init();
}

