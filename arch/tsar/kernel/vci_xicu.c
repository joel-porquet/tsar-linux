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
#include <linux/slab.h>
#include <linux/smp.h>

#include <asm/io.h>
#include <asm/smp_map.h>

#include "vci_xicu.h"

/*
 * Driver for the SoCLib VCI Generic Interrupt Controller Unit (soclib,vci_xicu)
 */

/* there is one xicu per cluster, at most */
struct vci_xicu *vci_xicu[MAX_NUMNODES];

#ifdef CONFIG_SOCLIB_VCI_XICU_HWI
/*
 * Irqchip driver for External Hardware Interrupts (HWI)
 *
 * Those interrupts can be routed to any processor connected to the XICU.
 */

static inline void __vci_xicu_hwi_generic_mask(struct irq_data *d, bool mask)
{
	struct vci_xicu *xicu;
	irq_hw_number_t hwirq;
	unsigned long node_hw_cpu, outirq;
	unsigned char cmd;

	/* unfortunately, the xicu does allow to (un)mask an HWI through its
	 * index directly. We have to (un)mask the HWI through the index of the
	 * local cpu the HWI is routed to */

	xicu = irq_data_get_irq_chip_data(d);
	hwirq = irqd_to_hwirq(d) - HWIRQ_START;

	raw_spin_lock(&xicu->lock);
	node_hw_cpu = xicu->hwi_to_hw_node_cpu[hwirq];

	BUG_ON(hwirq >= xicu->hwi_count);
	BUG_ON(node_hw_cpu > MAX_CPU_PER_CLUSTER);

	outirq = VCI_XICU_CPUID_MAP(node_hw_cpu);

	pr_debug("Node%d: (un)mask HWI %ld\n", xicu->node, hwirq);

	cmd = mask ? XICU_MSK_HWI_DISABLE : XICU_MSK_HWI_ENABLE;
	writel(BIT(hwirq), VCI_XICU_REG(xicu, cmd, outirq));
	raw_spin_unlock(&xicu->lock);
}

static void vci_xicu_hwi_mask(struct irq_data *d)
{
	__vci_xicu_hwi_generic_mask(d, true);
}

static void vci_xicu_hwi_unmask(struct irq_data *d)
{
	__vci_xicu_hwi_generic_mask(d, false);
}

#ifdef CONFIG_SMP
static int vci_xicu_hwi_set_affinity(struct irq_data *d,
		const cpumask_t *cpumask_req, bool force)
{
	struct vci_xicu *xicu;
	irq_hw_number_t hwirq;
	unsigned long chosen_cpu, hw_cpu, node_hw_cpu;
	cpumask_t cpumask_node_online;

	xicu = irq_data_get_irq_chip_data(d);
	hwirq = irqd_to_hwirq(d) - HWIRQ_START;

	/* get the online cpumask of the current cluster */
	cpumask_and(&cpumask_node_online, cpu_online_mask,
			cpumask_of_node(xicu->node));

	/* intersect the required mask with the cluster cpumask */
	chosen_cpu = cpumask_any_and(cpumask_req, &cpumask_node_online);

	if (chosen_cpu >= nr_cpu_ids) {
		/* try any cpu in the cluster, not even part of the required
		 * mask */
		chosen_cpu = cpumask_any(&cpumask_node_online);
		if (chosen_cpu >= nr_cpu_ids) {
			/* we have a problem! */
			return -EINVAL;
		}
	}

	pr_debug("Node%d: migrate HWI %ld to CPU%ld\n",
			xicu->node, hwirq,
			cpu_logical_map(chosen_cpu));

	/* disable the old association */
	vci_xicu_hwi_mask(d);

	/* update the association between the HWI and the newly cluster-local
	 * chosen cpu */
	raw_spin_lock(&xicu->lock);
	compute_hwcpuid(chosen_cpu, &hw_cpu, &node_hw_cpu);
	xicu->hwi_to_hw_node_cpu[hwirq] = node_hw_cpu;
	raw_spin_unlock(&xicu->lock);

	/* enable the new association */
	vci_xicu_hwi_unmask(d);

	/* update the affinity of the irq */
	cpumask_copy(d->affinity, &cpumask_node_online);

	return IRQ_SET_MASK_OK_NOCOPY;
}
#endif

static struct irq_chip vci_xicu_hwi_ctrl = {
	.name		= "vci_xicu_hwi",
	.irq_mask	= vci_xicu_hwi_mask,
	.irq_mask_ack	= vci_xicu_hwi_mask,
	.irq_unmask	= vci_xicu_hwi_unmask,
#ifdef CONFIG_SMP
	.irq_set_affinity = vci_xicu_hwi_set_affinity,
#endif
};
#endif /* CONFIG_SOCLIB_VCI_XICU_HWI */

/*
 * Irqchip driver for Writable-Triggered Interrupts (WTI/IPI) and Programmable
 * Timer Interrupts (PTI)
 *
 * Those interrupts are per processor: one WTI and one PTI for each processor.
 */

static inline void __vci_xicu_pcpu_generic_mask(struct irq_data *d, bool mask)
{
	struct vci_xicu *xicu;
	irq_hw_number_t hwirq;
	unsigned long hw_cpu, node_hw_cpu, outirq;
	unsigned char cmd;

	xicu = irq_data_get_irq_chip_data(d);
	hwirq = irqd_to_hwirq(d);
	compute_hwcpuid(smp_processor_id(), &hw_cpu, &node_hw_cpu);
	outirq = VCI_XICU_CPUID_MAP(node_hw_cpu);

	BUG_ON(hwirq >= MAX_PCPU_IRQS);

	switch (hwirq)
	{
#ifdef CONFIG_SMP
	case IPI_IRQ:
		pr_debug("CPU%ld: (un)mask IPI\n", hw_cpu);
		cmd = mask ? XICU_MSK_WTI_DISABLE : XICU_MSK_WTI_ENABLE;
		break;
#endif
	case PTI_IRQ:
		pr_debug("CPU%ld: (un)mask PTI\n", hw_cpu);
		cmd = mask ? XICU_MSK_PTI_DISABLE : XICU_MSK_PTI_ENABLE;
		break;
	}

	writel(BIT(node_hw_cpu), VCI_XICU_REG(xicu, cmd, outirq));
}

static void vci_xicu_pcpu_mask(struct irq_data *d)
{
	__vci_xicu_pcpu_generic_mask(d, true);
}

static void vci_xicu_pcpu_unmask(struct irq_data *d)
{
	__vci_xicu_pcpu_generic_mask(d, false);
}

static struct irq_chip vci_xicu_pcpu_ctrl = {
	.name		= "vci_xicu_pcpu",
	.irq_mask	= vci_xicu_pcpu_mask,
	.irq_mask_ack	= vci_xicu_pcpu_mask,
	.irq_unmask	= vci_xicu_pcpu_unmask,
};

/*
 * Irq domain driver
 */

static int vci_xicu_map(struct irq_domain *d, unsigned int virq,
		irq_hw_number_t hwirq)
{
	struct vci_xicu *xicu = d->host_data;

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_data(virq, xicu);

	switch (hwirq) {
#ifdef CONFIG_SMP
		case IPI_IRQ:
			/* the percpu_devid that is going to be used here is a
			 * fake one, but it is the only way to declare IPI IRQs
			 * as percpu IRQ and benefit from the kernel API */
			irq_set_percpu_devid(virq);
			irq_set_chip_and_handler(virq, &vci_xicu_pcpu_ctrl,
					handle_percpu_irq);
			break;
#endif
		case PTI_IRQ:
			/* the percpu_devid allocated here will be used later
			 * by the private timers, to point to a private
			 * clocksource instance */
			irq_set_percpu_devid(virq);
			irq_set_chip_and_handler(virq, &vci_xicu_pcpu_ctrl,
					handle_percpu_devid_irq);
			break;
		default:
#ifdef CONFIG_SOCLIB_VCI_XICU_HWI
			hwirq -= HWIRQ_START;
			if (hwirq < xicu->hwi_count) {
				/* regular HWI */
				irq_set_chip_and_handler(virq,
						&vci_xicu_hwi_ctrl,
						handle_level_irq);
			} else
#endif
			{
				pr_err("Cannot map HWI %ld\n", hwirq);
				return -EINVAL;
			}
			break;
	}

	return 0;
}

static int vci_xicu_xlate(struct irq_domain *d, struct device_node *ctrlr,
		const u32 *intspec, unsigned int intsize,
		unsigned long *out_hwirq, unsigned int *out_type)
{
	int ret;
	struct vci_xicu *xicu = d->host_data;

	/* get the HWI number from the device tree */
	ret = irq_domain_xlate_onecell(d, ctrlr, intspec, intsize,
			out_hwirq, out_type);
	if (!ret) {
		/* check the HWI number is legal */
		if (WARN_ON(*out_hwirq >= xicu->hwi_count))
			return -EINVAL;

		/* HWI numbering start after percpu IRQs (e.g. IPI and PTI) */
		*out_hwirq += HWIRQ_START;
	}
	return ret;
}

static struct irq_domain_ops vci_xicu_domain_ops = {
	.map	= vci_xicu_map,
	.xlate	= vci_xicu_xlate,
};

#ifdef CONFIG_SMP
SMP_IPI_CALL(vci_xicu_send_ipi)
{
	int cpu;

	/* make sure all memory stores are visible to the system before sending
	 * the IPI */
	wmb();

	/* send an IPI to all targeted cpus, via their cluster-local XICU */
	for_each_cpu(cpu, mask) {
		struct vci_xicu *xicu;
		unsigned long hw_cpu, node_hw_cpu;

		xicu = vci_xicu[cpu_to_node(cpu)];
		compute_hwcpuid(cpu, &hw_cpu, &node_hw_cpu);

		pr_debug("Send IPI to CPU%ld\n", hw_cpu);

		writel(val, VCI_XICU_REG(xicu, XICU_WTI_REG, node_hw_cpu));
	}
}
#endif

/*
 * ISR for the VCI_XICU
 * Read the priority encoder value from the cluster-local XICU: it can be
 * either a PTI, a HWIRQ (routed to the current cpu) or an IPI.
 */
static asmlinkage __irq_entry
HANDLE_IRQ(vci_xicu_handle_irq)
{
	struct vci_xicu *xicu;
	unsigned long hw_cpu, node_hw_cpu;
	unsigned long outirq;

	xicu = vci_xicu[numa_node_id()];
	compute_hwcpuid(smp_processor_id(), &hw_cpu, &node_hw_cpu);
	outirq = VCI_XICU_CPUID_MAP(node_hw_cpu);

	/* infinite loop until all active IRQs are processed */
	do {
		unsigned int prio;
		unsigned int virq;

		/* get the priority encoder for the current cpu */
		prio = readl(VCI_XICU_REG(xicu, XICU_PRIO, outirq));

		/* timer irq */
		if (XICU_PRIO_HAS_PTI(prio)) {
			/* only one possible timer */
			BUG_ON(XICU_PRIO_PTI(prio) != node_hw_cpu);
			virq = irq_find_mapping(xicu->irq_domain, PTI_IRQ);
			handle_IRQ(virq, regs);
			continue;
		}

#ifdef CONFIG_SOCLIB_VCI_XICU_HWI
		/* hardware irq */
		if (XICU_PRIO_HAS_HWI(prio)) {
			irq_hw_number_t hwirq;
			hwirq = XICU_PRIO_HWI(prio);
			virq = irq_find_mapping(xicu->irq_domain,
					hwirq + HWIRQ_START);
			handle_IRQ(virq, regs);
			continue;
		}
#endif

#ifdef CONFIG_SMP
		/* WTI/IPI irq */
		if (XICU_PRIO_HAS_WTI(prio)) {
			irq_hw_number_t wti = XICU_PRIO_WTI(prio);
			if (wti < MAX_CPU_PER_CLUSTER) {
				/* only one possible IPI */
				BUG_ON(wti != node_hw_cpu);
				virq = irq_find_mapping(xicu->irq_domain, IPI_IRQ);
				handle_IRQ(virq, regs);
			} else {
# ifdef CONFIG_SOCLIB_VCI_XICU_IOPIC
				/* give the wti to the iopic */
				wti -= MAX_CPU_PER_CLUSTER;
				vci_iopic_handle_irq(wti, regs);
# else
				BUG_ON(1);
				break;
# endif
			}
			continue;
		}
#endif
		break;
	} while (1);
}

#ifdef CONFIG_SMP
static irqreturn_t vci_xicu_ipi_interrupt(int irq, void *dev_id)
{
	struct vci_xicu *xicu;
	unsigned long hw_cpu, node_hw_cpu;

	/* acknowledge the IPI to our cluster-local xicu */
	xicu = vci_xicu[numa_node_id()];
	compute_hwcpuid(smp_processor_id(), &hw_cpu, &node_hw_cpu);
	readl(VCI_XICU_REG(xicu, XICU_WTI_REG, node_hw_cpu));

	pr_debug("CPU%ld: received an IPI\n", hw_cpu);

	handle_IPI();

	return IRQ_HANDLED;
}
#endif

void __init vci_xicu_state_init(struct vci_xicu *xicu)
{
	int cpu, hwi_ok = false;
	cpumask_t cpumask_node_possible;

	/*
	 * for a certain XICU and thus a certain cluster, mask all IRQs for the
	 * possible cpus of the same cluster
	 */
	cpumask_and(&cpumask_node_possible, cpu_possible_mask,
			cpumask_of_node(xicu->node));
	for_each_cpu(cpu, &cpumask_node_possible)
	{
		int i;
		unsigned long hw_cpu, node_hw_cpu, outirq;

		compute_hwcpuid(cpu, &hw_cpu, &node_hw_cpu);
		outirq = VCI_XICU_CPUID_MAP(node_hw_cpu);

		writel(~0, VCI_XICU_REG(xicu, XICU_MSK_WTI_DISABLE, outirq));
		writel(~0, VCI_XICU_REG(xicu, XICU_MSK_PTI_DISABLE, outirq));
		writel(~0, VCI_XICU_REG(xicu, XICU_MSK_HWI_DISABLE, outirq));
#if defined(CONFIG_SMP_IPI_BOOT)
		/* reenable IPI irqs if we need them for SMP bootup */
		writel(BIT(node_hw_cpu), VCI_XICU_REG(xicu,
					XICU_MSK_WTI_ENABLE, outirq));
#endif
#ifdef CONFIG_SOCLIB_VCI_XICU_HWI
		if (!hwi_ok) {
			/* route the HWI to the first cpu of the cluster */
			for (i = 0; i < MAX_HWI_COUNT; i++) {
				xicu->hwi_to_hw_node_cpu[i] = node_hw_cpu;
			}
			hwi_ok = true;
		}
#endif
	}
}

#ifdef CONFIG_SMP
/* percpu IRQs require a cookie - for IPI, such a cookie is not necessary but
 * we have to provide one */
static DEFINE_PER_CPU (int, fake_ipi_dev_id);

/* When booting/dying, a processor will have to enable/disable its IPI
 * interrupt provided by its cluster-local XICU */
static void vci_xicu_cpu_ipi_init(void)
{
	struct vci_xicu *xicu;

	xicu = vci_xicu[numa_node_id()];
	enable_percpu_irq(xicu->ipi_irq, IRQ_TYPE_NONE);
}

static void vci_xicu_cpu_ipi_stop(void)
{
	struct vci_xicu *xicu;

	xicu = vci_xicu[numa_node_id()];
	disable_percpu_irq(xicu->ipi_irq);
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

/* Notifier for secondary cpus to configure their percpu IPI interrupt. Set
 * high priority number so that IPIs are configured before PTIs. */
static struct notifier_block vci_xicu_ipi_notifier = {
	.notifier_call = vci_xicu_ipi_notify,
	.priority = 100,
};
#endif

int __init vci_xicu_init(struct device_node *of_node, struct device_node *parent)
{
	static bool __initdata first_call = true;
	struct vci_xicu *xicu;
	struct resource res;
	unsigned long xicu_config;
	unsigned long node;

	/* create a xicu object and map the device */
	BUG_ON(of_address_to_resource(of_node, 0, &res));
	BUG_ON(!request_mem_region(res.start, resource_size(&res), res.name));

	node = paddr_to_nid(res.start);
	BUG_ON(node >= num_online_nodes());

	xicu = kzalloc_node(sizeof(struct vci_xicu), GFP_KERNEL, node);
	BUG_ON(vci_xicu[node]);
	vci_xicu[node] = xicu;

	xicu->paddr = res.start;
	xicu->node = node;

	raw_spin_lock_init(&xicu->lock);

	xicu->virt = ioremap_nocache(res.start, resource_size(&res));
	BUG_ON(!xicu->virt);

	/* read the config from an hardware register */
	xicu_config = readl(VCI_XICU_REG(xicu, XICU_CONFIG, 0));

#ifdef CONFIG_SOCLIB_VCI_XICU_HWI
	xicu->hwi_count = XICU_CONFIG_HWI_COUNT(xicu_config);
#else
	xicu->hwi_count = 0;
#endif
	BUG_ON(xicu->hwi_count >= MAX_HWI_COUNT);

	/* configuration checking (IRQ_COUNT, WTI_COUNT and PTI_COUNT must
	 * match the number of cpus per cluster) */
	BUG_ON(XICU_CONFIG_IRQ_COUNT(xicu_config) <
			VCI_XICU_CPUID_MAP(MAX_CPU_PER_CLUSTER));
	BUG_ON(XICU_CONFIG_WTI_COUNT(xicu_config) < MAX_CPU_PER_CLUSTER);
	BUG_ON(XICU_CONFIG_PTI_COUNT(xicu_config) < MAX_CPU_PER_CLUSTER);

	/* add an irq domain for vci_xicu. Count 'hwi_count' and percpu IRQs as
	 * IRQ sources. Give the xicu object as host_data. */
	xicu->irq_domain = irq_domain_add_linear(of_node,
			MAX_PCPU_IRQS + xicu->hwi_count,
			&vci_xicu_domain_ops, xicu);
	BUG_ON(!xicu->irq_domain);

	vci_xicu_state_init(xicu);

#ifdef CONFIG_SMP
	/* create an irq association for IPI percpu irqs and provide an IRQ
	 * handler */
	xicu->ipi_irq = irq_create_mapping(xicu->irq_domain, IPI_IRQ);
	BUG_ON(request_percpu_irq(xicu->ipi_irq, vci_xicu_ipi_interrupt,
				"vci_xicu_per_cpu_ipi", &fake_ipi_dev_id));
#endif

	if (first_call) {
#ifdef CONFIG_SMP
		/* provide our callback for IPI function calls */
		set_smp_ipi_call(vci_xicu_send_ipi);

		/* the secondary cpus will enable their IPI when they boot */
		register_cpu_notifier(&vci_xicu_ipi_notifier);
#endif
		/* provide our IRQ handler */
		set_handle_irq(vci_xicu_handle_irq);

		first_call = false;
	}

#ifdef CONFIG_SMP
	/* enable the IPI of the boot cpu only when the xicu of the boot cpu
	 * has been created (which is not necessarily during the first call,
	 * since it depends in which order the device tree is analysed)
	 */
	if (numa_node_id() == node)
		vci_xicu_cpu_ipi_init();

#endif

	return 0;
}

IRQCHIP_DECLARE(vci_xicu, "soclib,vci_xicu", vci_xicu_init);
