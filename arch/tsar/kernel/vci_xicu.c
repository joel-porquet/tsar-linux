/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/bitops.h>
#include <linux/cpu.h>
#include <linux/ftrace.h>
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
 * Driver for the SoCLib VCI Generic Interrupt Controller Unit (soclib,vci_xicu)
 */

void __iomem *vci_xicu_virt_base;
struct irq_domain *vci_xicu_irq_domain;

/* number of HWIs (from peripheral devices) */
static unsigned long vci_xicu_hwi_count;

/* here we use a spinlock because we do not want to go to sleep when we can not
 * get it (like with mutex) */
static DEFINE_RAW_SPINLOCK(vci_xicu_lock);

static inline void vci_xicu_mask(struct irq_data *d)
{
	/* mask the requested irq (hwirq is the hardware irq number, local to
	 * the vci_xicu: i.e. 0 for the IPI, 1 for the PTI, 2-33 for the shared
	 * HWIs) */
	irq_hw_number_t hwirq = irqd_to_hwirq(d);

	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	BUG_ON(hwcpuid == INVALID_HWID);

	switch (hwirq)
	{
#ifdef CONFIG_SMP
		case VCI_XICU_IPI_PER_CPU_IRQ:
			/* mask the IPI IRQ for the targeted cpu */
			__raw_writel(BIT(hwcpuid),
					VCI_XICU_REG(XICU_MSK_WTI_DISABLE,
						hwcpuid));
			break;
#endif
		case VCI_XICU_PTI_PER_CPU_IRQ:
			/* mask the PTI IRQ for the targeted cpu */
			__raw_writel(BIT(hwcpuid),
					VCI_XICU_REG(XICU_MSK_PTI_DISABLE,
						hwcpuid));
			break;
		default:
			if ((hwirq >= VCI_XICU_MAX_PER_CPU_IRQ) && hwirq <
					(VCI_XICU_MAX_PER_CPU_IRQ +
					 vci_xicu_hwi_count)) {
				raw_spin_lock(&vci_xicu_lock);
				/* mask the specified HW IRQ for the targeted cpu */
				__raw_writel(BIT(hwirq - VCI_XICU_MAX_PER_CPU_IRQ),
						VCI_XICU_REG(XICU_MSK_HWI_DISABLE,
							hwcpuid));
				raw_spin_unlock(&vci_xicu_lock);
			} else {
				pr_warning("Cannot mask hwirq %ld\n", hwirq);
			}
			break;
	}
}

static inline void vci_xicu_unmask(struct irq_data *d)
{
	/* unmask the requested irq (hwirq is the hardware irq number, local to
	 * the vci_xicu: i.e. 0 for the IPI, 1 for the PTI, 2-33 for the shared
	 * HWIs) */
	irq_hw_number_t hwirq = irqd_to_hwirq(d);

	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	BUG_ON(hwcpuid == INVALID_HWID);

	switch (hwirq)
	{
#ifdef CONFIG_SMP
		case VCI_XICU_IPI_PER_CPU_IRQ:
			/* unmask the IPI IRQ for the targeted cpu */
			__raw_writel(BIT(hwcpuid),
					VCI_XICU_REG(XICU_MSK_WTI_ENABLE,
						hwcpuid));
			break;
#endif
		case VCI_XICU_PTI_PER_CPU_IRQ:
			/* unmask the PTI IRQ for the targeted cpu */
			__raw_writel(BIT(hwcpuid),
					VCI_XICU_REG(XICU_MSK_PTI_ENABLE,
						hwcpuid));
			break;
		default:
			if ((hwirq >= VCI_XICU_MAX_PER_CPU_IRQ) &&
					hwirq < (VCI_XICU_MAX_PER_CPU_IRQ +
						vci_xicu_hwi_count)) {
				raw_spin_lock(&vci_xicu_lock);
				/* unmask the specified HW IRQ for the targeted cpu */
				__raw_writel(BIT(hwirq - VCI_XICU_MAX_PER_CPU_IRQ),
						VCI_XICU_REG(XICU_MSK_HWI_ENABLE,
							hwcpuid));
				raw_spin_unlock(&vci_xicu_lock);
			} else {
				pr_warning("Cannot unmask hwirq %ld\n", hwirq);
			}
			break;
	}
}

#ifdef CONFIG_SMP
static int vci_xicu_set_affinity(struct irq_data *d,
		const struct cpumask *mask_val, bool force)
{
	int cpu;
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	unsigned int cpuid = cpumask_any_and(mask_val, cpu_online_mask);

	/* is there at least one online cpu, connected to the xicu, able to
	 * receive the irq */
	if (cpuid >= nr_cpu_ids)
		return -EINVAL;

	/* let us check that per-cpu irqs do not migrate */
	if (hwirq < VCI_XICU_MAX_PER_CPU_IRQ)
		return -EINVAL;

	raw_spin_lock(&vci_xicu_lock);

	/* mask the specified irq for all online cpus */
	for_each_cpu(cpu, cpu_online_mask)
		__raw_writel(BIT(hwirq - VCI_XICU_MAX_PER_CPU_IRQ),
				VCI_XICU_REG(XICU_MSK_HWI_DISABLE,
					cpu_logical_map(cpu)));
	/* unmask the specified irq for the specified cpu */
	__raw_writel(BIT(hwirq - VCI_XICU_MAX_PER_CPU_IRQ),
			VCI_XICU_REG(XICU_MSK_HWI_ENABLE,
				cpu_logical_map(cpuid)));

	raw_spin_unlock(&vci_xicu_lock);

	return IRQ_SET_MASK_OK;
}
#endif

static struct irq_chip vci_xicu_controller = {
	.name		= "vci_xicu",
	.irq_mask	= vci_xicu_mask,
	.irq_mask_ack	= vci_xicu_mask,
	.irq_unmask	= vci_xicu_unmask,
#ifdef CONFIG_SMP
	.irq_set_affinity = vci_xicu_set_affinity,
#endif
};

static int vci_xicu_map(struct irq_domain *d, unsigned int virq,
		irq_hw_number_t hwirq)
{
	irq_set_status_flags(virq, IRQ_LEVEL);

	switch (hwirq) {
#ifdef CONFIG_SMP
		case VCI_XICU_IPI_PER_CPU_IRQ:
			/* percpu handler without percpu_devid */
			irq_set_chip_and_handler(virq, &vci_xicu_controller,
					handle_percpu_irq);
			break;
#endif
		case VCI_XICU_PTI_PER_CPU_IRQ:
			/* the percpu_devid allocated here will be used later
			 * by the private timers, to point to a private
			 * clocksource instance */
			irq_set_percpu_devid(virq);
			irq_set_chip_and_handler(virq, &vci_xicu_controller,
					handle_percpu_devid_irq);
			break;
		default:
			if ((hwirq >= VCI_XICU_MAX_PER_CPU_IRQ) &&
					hwirq < (VCI_XICU_MAX_PER_CPU_IRQ +
						vci_xicu_hwi_count))
				/* regular hwirqs */
				irq_set_chip_and_handler(virq,
						&vci_xicu_controller,
						handle_level_irq);
			else
				pr_warning("Cannot map hwirq %ld\n", hwirq);
			break;
	}

	return 0;
}

static struct irq_domain_ops vci_xicu_domain_ops = {
	.map	= vci_xicu_map,
	.xlate	= irq_domain_xlate_onecell,
};

#ifdef CONFIG_SMP
SMP_IPI_CALL(vci_xicu_send_ipi)
{
	int cpu;

	/* make sure all memory stores are visible to the system before sending
	 * the ipi */
	wmb();

	/* send an IPI to all targeted cpus */
	for_each_cpu(cpu, mask) {
		unsigned long hwcpuid = cpu_logical_map(cpu);
		BUG_ON(hwcpuid == INVALID_HWID);
		__raw_writel(0, VCI_XICU_REG(XICU_WTI_REG, hwcpuid));
	}
}
#endif

/*
 * ISR for the VCI_XICU
 * Read the priority encoder value: it can be either an IRQ from the private
 * PTI, an external HWIRQ (routed on the current cpu) or an IPI.
 */
static asmlinkage __irq_entry
HANDLE_IRQ(vci_xicu_handle_irq)
{
	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	unsigned int prio;
	unsigned int virq;

	BUG_ON(hwcpuid == INVALID_HWID);

	do {
		/* get the priority encoder for the current cpu */
		prio = __raw_readl(VCI_XICU_REG(XICU_PRIO, hwcpuid));

		/* timer irq */
		if (XICU_PRIO_HAS_PTI(prio)) {
			/* there should be only one timer possible for this cpu */
			BUG_ON(XICU_PRIO_PTI(prio) != hwcpuid);
			virq = irq_find_mapping(vci_xicu_irq_domain,
					VCI_XICU_PTI_PER_CPU_IRQ);
			handle_IRQ(virq, regs);
			continue;
		}

		/* hardware irq */
		if (XICU_PRIO_HAS_HWI(prio)) {
			unsigned int hwirq;
			hwirq = XICU_PRIO_HWI(prio);
			virq = irq_find_mapping(vci_xicu_irq_domain,
					hwirq + VCI_XICU_MAX_PER_CPU_IRQ);
			handle_IRQ(virq, regs);
			continue;
		}

#ifdef CONFIG_SMP
		/* IPI irq */
		if (XICU_PRIO_HAS_WTI(prio)) {
			/* there should be only one ipi possible for this cpu */
			BUG_ON(XICU_PRIO_WTI(prio) != hwcpuid);
			virq = irq_find_mapping(vci_xicu_irq_domain,
					VCI_XICU_IPI_PER_CPU_IRQ);
			handle_IRQ(virq, regs);
			continue;
		}
#endif
		break;
	} while (1);
}

#ifdef CONFIG_SMP
static irqreturn_t vci_xicu_ipi_interrupt(int irq, void *dev_id)
{
	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	BUG_ON(hwcpuid == INVALID_HWID);

	/* acknowledge the IPI */
	__raw_readl(VCI_XICU_REG(XICU_WTI_REG, hwcpuid));

	handle_IPI();

	return IRQ_HANDLED;
}
#endif

void __init vci_xicu_mask_init(void)
{
	int cpu;
	unsigned int i;


	/* mask all IPIs, PTIs and HWIs */
	for_each_cpu(cpu, cpu_possible_mask)
	{
#ifdef CONFIG_SMP
		__raw_writel(0, VCI_XICU_REG(XICU_MSK_WTI_ENABLE,
					cpu_logical_map(cpu)));
#endif
		__raw_writel(0, VCI_XICU_REG(XICU_MSK_PTI_DISABLE,
					cpu_logical_map(cpu)));
		for (i = 0; i < vci_xicu_hwi_count; i++)
			__raw_writel(BIT(i), VCI_XICU_REG(XICU_MSK_HWI_DISABLE,
						cpu_logical_map(cpu)));
	}
}

#ifdef CONFIG_SMP
static unsigned int vci_xicu_ipi_irq;

static void vci_xicu_cpu_ipi_init(void)
{
	enable_percpu_irq(vci_xicu_ipi_irq, 0);
}

static void vci_xicu_cpu_ipi_stop(void)
{
	disable_percpu_irq(vci_xicu_ipi_irq);
}

static int vci_xicu_ipi_notify(struct notifier_block *self,
		unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
		case CPU_STARTING:
			vci_xicu_cpu_ipi_init();
			break;
		case CPU_DYING:
			vci_xicu_cpu_ipi_stop();
			break;
	}
	return NOTIFY_OK;
}

/* Notifier for configuring the XICU from non-boot cpus. Set high priority
 * number so the xicu is configured before the private PTIs. */
static struct notifier_block vci_xicu_ipi_notifier = {
	.notifier_call = vci_xicu_ipi_notify,
	.priority = 100,
};
#endif

int __init vci_xicu_init(struct device_node *of_node, struct device_node *parent)
{
	struct resource res;
	unsigned long xicu_config;

	BUG_ON(of_address_to_resource(of_node, 0, &res));
	BUG_ON(!request_mem_region(res.start, resource_size(&res), res.name));

	vci_xicu_virt_base = ioremap_nocache(res.start, resource_size(&res));

	BUG_ON(!vci_xicu_virt_base);

	xicu_config = __raw_readl(VCI_XICU_REG(XICU_CONFIG, 0));
	vci_xicu_hwi_count = XICU_CONFIG_HWI_COUNT(xicu_config);

	/* configuration checking (IRQ_COUNT, WTI_COUNT and PTI_COUNT must
	 * match the number of cpus) */
	BUG_ON(XICU_CONFIG_IRQ_COUNT(xicu_config) < num_possible_cpus());
	BUG_ON(XICU_CONFIG_WTI_COUNT(xicu_config) < num_possible_cpus());
	BUG_ON(XICU_CONFIG_PTI_COUNT(xicu_config) < num_possible_cpus());

	/* add an irq domain for vci_xicu (we count vci_xicu_hwi_count and
	 * VCI_XICU_MAX_PER_CPU_IRQ irq sources) */
	vci_xicu_irq_domain = irq_domain_add_linear(of_node,
			vci_xicu_hwi_count + VCI_XICU_MAX_PER_CPU_IRQ,
			&vci_xicu_domain_ops, NULL);
	BUG_ON(!vci_xicu_irq_domain);

	/* disable all IRQs in the XICU */
	vci_xicu_mask_init();

#ifdef CONFIG_SMP
	/* provide our callback for IPI function calls */
	set_smp_ipi_call(vci_xicu_send_ipi);

	/* create an irq association for IPI percpu irqs and provide an IRQ
	 * handler */
	vci_xicu_ipi_irq = irq_create_mapping(vci_xicu_irq_domain,
			VCI_XICU_IPI_PER_CPU_IRQ);
	BUG_ON(request_percpu_irq(vci_xicu_ipi_irq, vci_xicu_ipi_interrupt,
				"vci_xicu_per_cpu_ipi",
				NULL));

	/* Immediately enable the IPI irq for the boot cpu */
	vci_xicu_cpu_ipi_init();

	/* enable IPI irqs for other cpus when they boot */
	register_cpu_notifier(&vci_xicu_ipi_notifier);

	/* set the default affinity to the boot cpu */
	cpumask_clear(irq_default_affinity);
	cpumask_set_cpu(smp_processor_id(), irq_default_affinity);
#endif

	/* provide an IRQ handler for the vci_xicu */
	set_handle_irq(vci_xicu_handle_irq);

	return 0;
}

IRQCHIP_DECLARE(vci_xicu, "soclib,vci_xicu", vci_xicu_init);
