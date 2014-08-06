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

#include <asm/io.h>
#include <asm/smp_map.h>

#include "vci_xicu.h"

/*
 * Driver for the SoCLib VCI XICU Timer (soclib,vci_xicu_timer)
 */

static unsigned long vci_xicu_timer_period;
static unsigned long clk_rate;

static struct clock_event_device __percpu *vci_xicu_clockevent;

static unsigned int vci_xicu_timer_irq;

static int vci_xicu_timer_set_next_event(unsigned long delta,
		struct clock_event_device *evt)
{
	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	BUG_ON(hwcpuid == INVALID_HWCPUID);

	/* setup timer for one shot */
	__raw_writel(0xffffffff, VCI_XICU_REG(XICU_PTI_PER, hwcpuid));
	__raw_writel(delta, VCI_XICU_REG(XICU_PTI_VAL, hwcpuid));

	return 0;
}

static void vci_xicu_timer_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt)
{
	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	BUG_ON(hwcpuid == INVALID_HWCPUID);

	if (mode == CLOCK_EVT_MODE_PERIODIC)
	{
		/* setup timer for periodic ticks */
		__raw_writel(vci_xicu_timer_period,
				VCI_XICU_REG(XICU_PTI_PER, hwcpuid));
		__raw_writel(vci_xicu_timer_period,
				VCI_XICU_REG(XICU_PTI_VAL, hwcpuid));
	} else {
		/* disable timer */
		__raw_writel(0, VCI_XICU_REG(XICU_PTI_PER, hwcpuid));
		__raw_writel(0, VCI_XICU_REG(XICU_PTI_VAL, hwcpuid));
	}
}

static irqreturn_t vci_xicu_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	unsigned long hwcpuid = cpu_logical_map(smp_processor_id());

	BUG_ON(hwcpuid == INVALID_HWCPUID);

	if (evt->mode == CLOCK_EVT_MODE_ONESHOT)
	{
		/* if one-shot, ack and deactivate the IRQ */
		pr_debug("CPU%ld: one-shot time INT at cycle %ld\n",
				hwcpuid, read_c0_count());
		__raw_writel(0, VCI_XICU_REG(XICU_PTI_PER, hwcpuid));
	} else {
		/* otherwise just ack the IRQ */
		pr_debug("CPU%ld: periodic time INT at cycle %ld\n",
				hwcpuid, read_c0_count());
		__raw_readl(VCI_XICU_REG(XICU_PTI_ACK, hwcpuid));
	}

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static void vci_xicu_cpu_timer_init(struct clock_event_device *evt)
{
	int cpu = smp_processor_id();

	evt->name = "vci_xicu_per_cpu_timer";
	evt->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT |
		CLOCK_EVT_FEAT_PERCPU;
	evt->set_mode = vci_xicu_timer_set_mode;
	evt->set_next_event = vci_xicu_timer_set_next_event;
	evt->cpumask = cpumask_of(cpu);
	evt->rating = 300;
	evt->irq = vci_xicu_timer_irq;

	clockevents_config_and_register(evt, clk_rate,
			1, 0xffffffff);
	enable_percpu_irq(evt->irq, 0);
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

static struct notifier_block vci_xicu_timer_notifier = {
	.notifier_call = vci_xicu_timer_notify,
};
#endif

static unsigned long __init vci_xicu_timer_get_clock(struct device_node *of_node)
{
	static struct clk *clk;

	/* get the clock associated to the timer */
	clk = of_clk_get(of_node, 0);

	BUG_ON(IS_ERR(clk));

	BUG_ON(clk_prepare_enable(clk));

	return clk_get_rate(clk);
}

static void __init vci_xicu_timer_init(struct device_node *of_node)
{
	/*
	 * note: the component has already been mapped as an
	 * interrupt-controller
	 */

	clk_rate = vci_xicu_timer_get_clock(of_node);

	BUG_ON(!clk_rate);

	/* compute the tick value in terms of cycle */
	vci_xicu_timer_period = DIV_ROUND_CLOSEST(clk_rate, HZ);

	vci_xicu_timer_irq = irq_create_mapping(vci_xicu_irq_domain,
			VCI_XICU_PTI_PER_CPU_IRQ);

#ifdef CONFIG_SMP
	register_cpu_notifier(&vci_xicu_timer_notifier);
#endif

	vci_xicu_clockevent = alloc_percpu(struct clock_event_device);

	BUG_ON(request_percpu_irq(vci_xicu_timer_irq, vci_xicu_timer_interrupt,
				"vci_xicu_per_cpu_timer",
				vci_xicu_clockevent));

	/* Immediately configure the timer on the boot cpu */
	vci_xicu_cpu_timer_init(this_cpu_ptr(vci_xicu_clockevent));

}

CLOCKSOURCE_OF_DECLARE(vci_xicu_timer, "soclib,vci_xicu_timer", vci_xicu_timer_init);
