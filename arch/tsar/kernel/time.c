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

void __init time_init(void)
{
	/* parse clk nodes and register drivers */
	of_clk_init(NULL);
	/* parse clocksource nodes */
	clocksource_of_init();
}

