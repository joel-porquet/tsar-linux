/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/io.h>

/*
 * Driver for the SoCLib VCI Timer (vci_timer)
 */

/* vci_timer register map */
#define VCI_TIMER_VALUE		0x0 // RO: ever-counting counter
#define VCI_TIMER_MODE		0x4 // WO: [1:0] = [TIMER_IRQ_ENABLED:TIMER_RUNNING]
#define VCI_TIMER_PERIOD	0x8 // RW: timer period
#define VCI_TIMER_RESETIRQ	0xc // RW: reset IRQ (returns !=0 on read when pending IRQ)

/* TIMER_MODE bitfield */
#define VCI_TIMER_RUNNING	(_AC(1, UL) << 0)
#define VCI_TIMER_IRQ_ENABLED	(_AC(1, UL) << 1)

static struct clk *vci_timer_clk;
static unsigned long vci_timer_rate;
static unsigned long vci_timer_reload;

static void __iomem *vci_timer_virt_base;

/* clockevent configuration */
static inline int vci_timer_ack(void)
{
	if(__raw_readl(vci_timer_virt_base + VCI_TIMER_RESETIRQ)) {
		/* there is an IRQ pending, reset it */
		__raw_writel(1, vci_timer_virt_base + VCI_TIMER_RESETIRQ);
		return 1;
	}

	return 0;
}

static irqreturn_t vci_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = (struct clock_event_device*)dev_id;

	if (vci_timer_ack()) {
		evt->event_handler(evt);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static void vci_timer_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt)
{
	unsigned long ctrl = 0;

	switch (mode) {
		case CLOCK_EVT_MODE_PERIODIC:
			/* that's the only mode the vci_time supports,
			 * set a period of one tick (rate/HZ), with IRQ enabled */
			ctrl = VCI_TIMER_RUNNING | VCI_TIMER_IRQ_ENABLED;
			__raw_writel(vci_timer_reload,
					vci_timer_virt_base + VCI_TIMER_PERIOD);
			break;
		case CLOCK_EVT_MODE_ONESHOT:
		case CLOCK_EVT_MODE_SHUTDOWN:
		case CLOCK_EVT_MODE_RESUME:
		case CLOCK_EVT_MODE_UNUSED:
		default:
			break;
	}

	__raw_writel(ctrl, vci_timer_virt_base + VCI_TIMER_MODE);
}

static struct clock_event_device vci_timer_clockevent = {
	.features = CLOCK_EVT_FEAT_PERIODIC,
	.set_mode = vci_timer_set_mode,
	.rating = 300,
};

static struct irqaction vci_timer_irq = {
	.name		= "vci_timer",
	.flags		= IRQF_TIMER,
	/* everybody also defines IRQF_DISABLED but it's supposed to be
	 * deprecated... */
	.handler	= vci_timer_interrupt,
	.dev_id		= &vci_timer_clockevent,
};

static void __init vci_timer_clockevent_init(unsigned int irq)
{
	/* compute the tick value in terms of cycle */
	vci_timer_reload = DIV_ROUND_CLOSEST(vci_timer_rate, HZ);

	/* setup the timer IRQ handler */
	setup_irq(irq, &vci_timer_irq);

	/* register the vci timer as a clockevent device,
	 * min_ and max_delta are 0 because the vci timer doesn't support one
	 * shot mode */
	clockevents_config_and_register(&vci_timer_clockevent,
			vci_timer_rate, 0, 0);
}

/* clocksource configuration */
static void __init vci_timer_clocksource_init(void)
{
	/* declare the vci_timer as a simple memory mapped clocksource */
	clocksource_mmio_init(vci_timer_virt_base + VCI_TIMER_VALUE,
			"vci_timer_clocksource", vci_timer_rate,
			300, 32, clocksource_mmio_readl_up);
}

static void __init vci_timer_get_clock(struct device_node *of_node)
{
	int err;

	/* get the clock associated to the timer */
	vci_timer_clk = of_clk_get(of_node, 0);

	if (IS_ERR(vci_timer_clk)) {
		pr_err("vci_timer: clock not found (%ld)\n",
				PTR_ERR(vci_timer_clk));
		return;
	}

	err = clk_prepare_enable(vci_timer_clk);
	if (err) {
		pr_err("vci_timer: clock failed to prepare and enable (%d)\n", err);
		clk_put(vci_timer_clk);
		return;
	}

	vci_timer_rate = clk_get_rate(vci_timer_clk);
}

static void __init vci_timer_init(struct device_node *of_node)
{
	unsigned int irq;
	struct resource res;

	/* get virq of the irq that links the vci_timer to the parent icu (ie
	 * vci_icu) */
	irq = irq_of_parse_and_map(of_node, 0);
	if (!irq)
		panic("%s: failed to get IRQ\n", of_node->full_name);

	if (of_address_to_resource(of_node, 0, &res))
		panic("%s: failed to get memory range\n", of_node->full_name);

	if (!request_mem_region(res.start, resource_size(&res), res.name))
		panic("%s: failed to request memory\n", of_node->full_name);

	vci_timer_virt_base = ioremap_nocache(res.start, resource_size(&res));

	if (!vci_timer_virt_base)
		panic("%s: failed to remap memory\n", of_node->full_name);

	/* get the clock-frequency of the timer (and thus the system) */
	vci_timer_get_clock(of_node);

	if (!vci_timer_rate)
		panic("%s: failed to determine clock frequency\n",
				of_node->full_name);

	/* disable timer */
	__raw_writel(0, vci_timer_virt_base + VCI_TIMER_MODE);

	/* initialize the clocksource (ever-counting counter)
	 * and the clockevent (periodic timer) */
	vci_timer_clocksource_init();
	vci_timer_clockevent_init(irq);
}
CLOCKSOURCE_OF_DECLARE(vci_timer, "soclib,vci_timer", vci_timer_init);

