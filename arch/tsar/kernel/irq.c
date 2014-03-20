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
 * handle_IRQ is called after decoding the IRQ by the system ICU (vci_icu)
 */
void handle_IRQ(unsigned int virq, struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	irq_enter();

	generic_handle_irq(virq);

	irq_exit();
	set_irq_regs(old_regs);
}

/*
 * Allow the system ICU to provide its own IRQ handler
 */

handle_irq_t *handle_irq_icu;

void __init set_handle_irq(handle_irq_t *new)
{
	if (handle_irq_icu)
		return;

	handle_irq_icu = new;
}

void __init mips32_icu_init(void)
{
	/* unmask all IRQ sources of the mips32 */
	set_c0_status(ST0_IM);
}

/*
 * IRQ global initialization
 */

void __init init_IRQ(void)
{
	/* scan through the device tree and find ICUs */
	irqchip_init();

	/* check that we found at least one ICU to provide a IRQ handler */
	if (!handle_irq_icu)
		panic("No interrupt controller found!");

	/* unmask the internal IRQ sources for the boot cpu */
	mips32_icu_init();
}

