/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/io.h>
#include <asm/smp_map.h>

#include "vci_xicu.h"

/*
 * Driver for the SoCLib VCI XICU Timer (soclib,vci_xicu_timer)
 */

static struct clock_event_device __percpu *vci_xicu_clockevent;

static int vci_xicu_timer_set_next_event(unsigned long delta, struct
		clock_event_device *evt)
{
	int cpu = smp_processor_id();
	struct vci_xicu *xicu;
	unsigned long hw_cpu, node_hw_cpu;

	compute_hwcpuid(cpu, &hw_cpu, &node_hw_cpu);
	xicu = vci_xicu[cpu_to_node(cpu)];

	/* setup timer for one shot with the specified delta */
	writel(ULONG_MAX, VCI_XICU_REG(xicu, XICU_PTI_PER, node_hw_cpu));
	writel(delta, VCI_XICU_REG(xicu, XICU_PTI_VAL, node_hw_cpu));

	return 0;
}

static void vci_xicu_timer_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt)
{
	int cpu = smp_processor_id();
	struct vci_xicu *xicu;
	unsigned long hw_cpu, node_hw_cpu;

	compute_hwcpuid(cpu, &hw_cpu, &node_hw_cpu);
	xicu = vci_xicu[cpu_to_node(cpu)];

	if (mode == CLOCK_EVT_MODE_PERIODIC) {
		writel(xicu->clk_period, VCI_XICU_REG(xicu, XICU_PTI_PER,
					node_hw_cpu));
		writel(xicu->clk_period, VCI_XICU_REG(xicu, XICU_PTI_VAL,
					node_hw_cpu));
	} else {
		/* disable timer */
		writel(0, VCI_XICU_REG(xicu, XICU_PTI_PER, node_hw_cpu));
		writel(0, VCI_XICU_REG(xicu, XICU_PTI_VAL, node_hw_cpu));
	}
}

static irqreturn_t vci_xicu_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	int cpu = smp_processor_id();
	struct vci_xicu *xicu;
	unsigned long hw_cpu, node_hw_cpu;

	compute_hwcpuid(cpu, &hw_cpu, &node_hw_cpu);
	xicu = vci_xicu[cpu_to_node(cpu)];

	if (evt->mode == CLOCK_EVT_MODE_ONESHOT) {
		/* if one-shot, ack and deactivate the IRQ by writing */
		pr_debug("CPU%ld: one-shot time INT at cycle %ld\n",
				hw_cpu, read_c0_count());
		writel(0, VCI_XICU_REG(xicu, XICU_PTI_PER, node_hw_cpu));
	} else {
		/* otherwise just ack the IRQ by reading */
		pr_debug("CPU%ld: periodic time INT at cycle %ld\n",
				hw_cpu, read_c0_count());
		readl(VCI_XICU_REG(xicu, XICU_PTI_ACK, node_hw_cpu));
	}

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static void vci_xicu_cpu_timer_init(struct clock_event_device *evt)
{
	struct vci_xicu *xicu;
	int cpu = smp_processor_id();

	xicu = vci_xicu[cpu_to_node(cpu)];

	evt->name = "vci_xicu_per_cpu_timer";
	evt->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT |
		CLOCK_EVT_FEAT_PERCPU;
	evt->set_mode = vci_xicu_timer_set_mode;
	evt->set_next_event = vci_xicu_timer_set_next_event;
	evt->cpumask = cpumask_of(cpu);
	evt->rating = 300;
	evt->irq = xicu->timer_irq;

	clockevents_config_and_register(evt, xicu->clk_rate, 1, ULONG_MAX);
	enable_percpu_irq(evt->irq, IRQ_TYPE_NONE);
}

#ifdef CONFIG_SMP
static void vci_xicu_cpu_timer_stop(struct clock_event_device *evt)
{
	evt->set_mode(CLOCK_EVT_MODE_UNUSED, evt);
	disable_percpu_irq(evt->irq);
}

static int vci_xicu_timer_notify(struct notifier_block *self,
		unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
		case CPU_STARTING:
			vci_xicu_cpu_timer_init(this_cpu_ptr(vci_xicu_clockevent));
			break;
		case CPU_DYING:
			vci_xicu_cpu_timer_stop(this_cpu_ptr(vci_xicu_clockevent));
			break;
	}
	return NOTIFY_OK;
}

/* Notifier for secondary cpus to configure their percpu PTI interrupt */
static struct notifier_block vci_xicu_timer_notifier = {
	.notifier_call = vci_xicu_timer_notify,
};
#endif

static unsigned long __init vci_xicu_timer_get_clock(struct device_node *of_node)
{
	static struct clk *clk;

	clk = of_clk_get(of_node, 0);
	BUG_ON(IS_ERR(clk));
	BUG_ON(clk_prepare_enable(clk));
	return clk_get_rate(clk);
}

static void __init vci_xicu_timer_init(struct device_node *of_node)
{
	static bool __initdata first_call = true;
	struct resource res;
	struct vci_xicu *xicu;
	unsigned long node;

	/*
	 * note: the component has already been mapped as an
	 * interrupt-controller
	 */

	BUG_ON(of_address_to_resource(of_node, 0, &res));
	node = paddr_to_nid(res.start);
	xicu = vci_xicu[node];
	BUG_ON(!xicu);

	xicu->clk_rate = vci_xicu_timer_get_clock(of_node);
	BUG_ON(!xicu->clk_rate);
	xicu->clk_period = DIV_ROUND_CLOSEST(xicu->clk_rate, HZ);

	xicu->timer_irq = irq_create_mapping(xicu->irq_domain, PTI_IRQ);

	if (first_call) {
		vci_xicu_clockevent = alloc_percpu(struct clock_event_device);
#ifdef CONFIG_SMP
		/* the secondary cpus will enable their timer when they boot */
		register_cpu_notifier(&vci_xicu_timer_notifier);
#endif
		first_call = false;
	}

	BUG_ON(request_percpu_irq(xicu->timer_irq, vci_xicu_timer_interrupt,
				"vci_xicu_per_cpu_timer",
				vci_xicu_clockevent));

	/* configure the timer of the boot cpu only when the xicu of the boot
	 * cpu has been created (which is not necessarily during the first
	 * call, since it depends in which order the device tree is analysed)
	 */
	if (numa_node_id() == node)
		vci_xicu_cpu_timer_init(this_cpu_ptr(vci_xicu_clockevent));
}

CLOCKSOURCE_OF_DECLARE(vci_xicu_timer, "soclib,vci_xicu_timer", vci_xicu_timer_init);
