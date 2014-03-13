/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/clk-provider.h>
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/of.h>

/*
 * Driver for the internal cycle count of the MIPS32 (soclib,mips32_clksrc)
 *
 * Note that this driver relies on a couple strong assumptions:
 * - all the cpus boot up at the same time, and belong to the same clock domain
 *   (so their counters are always equal at runtime).
 * - cpus can not go to sleep and stop their internal counters, to avoid any
 *   desynchronization.
 *
 * Ideally, TSAR should have a RTC or something likewise that would give us an
 * accurate value of the time in the system (which the XICU does not provide).
 */

static cycle_t mips32_clksrc_read(struct clocksource *cs)
{
	/* read from the cycle count register of the MIPS32 */
	return (cycle_t) read_c0_count();
}

static struct clocksource mips32_clksrc_cs = {
	.name	= "mips32_clksrc",
	.rating	= 400,
	.read	= mips32_clksrc_read,
	.mask	= CLOCKSOURCE_MASK(32),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static unsigned long __init mips32_clksrc_get_clock(struct device_node *of_node)
{
	int err;
	static struct clk *clk;

	/* get the clock associated to mips32_clksrc */
	clk = of_clk_get(of_node, 0);

	if (IS_ERR(clk)) {
		pr_err("mips32_clksrc: clock not found (%ld)\n",
				PTR_ERR(clk));
		return 0;
	}

	err = clk_prepare_enable(clk);
	if (err) {
		pr_err("mips32_clksrc: clock failed to prepare and enable (%d)\n", err);
		clk_put(clk);
		return 0;
	}

	return clk_get_rate(clk);
}

static void __init mips32_clksrc_init(struct device_node *of_node)
{
	unsigned long clk_rate;

	clk_rate = mips32_clksrc_get_clock(of_node);

	if (!clk_rate)
		panic("%s: failed to determine clock frequency\n",
				of_node->full_name);

	clocksource_register_hz(&mips32_clksrc_cs, clk_rate);
}
CLOCKSOURCE_OF_DECLARE(mips32_clksrc, "soclib,mips32_clksrc", mips32_clksrc_init);


/*
 * Time initialization at startup
 */

void __init time_init(void)
{
	/* parse clk nodes and register drivers */
	of_clk_init(NULL);
	/* parse clocksource nodes */
	clocksource_of_init();
}

