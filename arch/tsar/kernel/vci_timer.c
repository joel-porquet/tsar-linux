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
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/io.h>

/*
 * Driver for the SoCLib VCI Timer (soclib,vci_timer)
 */

/* vci_timer register map */
#define VCI_TIMER_VALUE		0x0 // RO: ever-counting counter
#define VCI_TIMER_MODE		0x4 // WO: [1:0] = [TIMER_IRQ_ENABLED:TIMER_RUNNING]
#define VCI_TIMER_PERIOD	0x8 // RW: timer period
#define VCI_TIMER_RESETIRQ	0xc // RW: reset IRQ (returns !=0 on read when pending IRQ)

/* TIMER_MODE bitfield */
#define VCI_TIMER_RUNNING	(_AC(1, UL) << 0)
#define VCI_TIMER_IRQ_ENABLED	(_AC(1, UL) << 1)

static unsigned long vci_timer_period;

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
			__raw_writel(vci_timer_period,
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
	.handler	= vci_timer_interrupt,
	.dev_id		= &vci_timer_clockevent,
};

static void __init vci_timer_clockevent_init(unsigned int irq,
		unsigned long clk_rate)
{
	/* compute the tick value in terms of cycle */
	vci_timer_period = DIV_ROUND_CLOSEST(clk_rate, HZ);

	/* setup the timer IRQ handler */
	setup_irq(irq, &vci_timer_irq);

	/* register the vci timer as a clockevent device,
	 * min_ and max_delta are 0 because the vci timer doesn't support one
	 * shot mode */
	clockevents_config_and_register(&vci_timer_clockevent,
			clk_rate, 0, 0);
}

/* clocksource configuration */
static void __init vci_timer_clocksource_init(unsigned long clk_rate)
{
	/* declare the vci_timer as a simple memory mapped clocksource */
	clocksource_mmio_init(vci_timer_virt_base + VCI_TIMER_VALUE,
			"vci_timer_clocksource", clk_rate,
			300, 32, clocksource_mmio_readl_up);
}

static unsigned long __init vci_timer_get_clock(struct device_node *of_node)
{
	static struct clk *clk;

	/* get the clock associated to the timer */
	clk = of_clk_get(of_node, 0);

	BUG_ON(IS_ERR(clk));
	BUG_ON(clk_prepare_enable(clk));

	return clk_get_rate(clk);
}

static void __init vci_timer_init(struct device_node *of_node)
{
	unsigned int irq;
	struct resource res;
	unsigned long clk_rate;

	/* get virq of the irq that links the vci_timer to the parent icu (e.g.
	 * vci_icu) */
	irq = irq_of_parse_and_map(of_node, 0);
	BUG_ON(!irq);

	BUG_ON(of_address_to_resource(of_node, 0, &res));

	BUG_ON(!request_mem_region(res.start, resource_size(&res), res.name));

	vci_timer_virt_base = ioremap_nocache(res.start, resource_size(&res));

	BUG_ON(!vci_timer_virt_base);

	/* get the clock-frequency of the timer (and thus the system) */
	clk_rate = vci_timer_get_clock(of_node);

	BUG_ON(!clk_rate);

	/* disable timer */
	__raw_writel(0, vci_timer_virt_base + VCI_TIMER_MODE);

	/* initialize the clocksource (ever-counting counter)
	 * and the clockevent (periodic timer) */
	vci_timer_clocksource_init(clk_rate);
	vci_timer_clockevent_init(irq, clk_rate);
}

CLOCKSOURCE_OF_DECLARE(vci_timer, "soclib,vci_timer", vci_timer_init);
