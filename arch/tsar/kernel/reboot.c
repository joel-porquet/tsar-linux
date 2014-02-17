/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/pm.h>
#include <linux/reboot.h>

void machine_power_off(void)
{
	pr_info("requested machine_power_off");
	while(1);
}

void machine_halt(void)
{
	pr_info("requested machine_halt");
	while(1);
}

void machine_restart(char *cmd)
{
	pr_info("requested machine_restart");
	while(1);
}

void (*pm_power_off)(void) = machine_power_off;
EXPORT_SYMBOL(pm_power_off);

