/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/node.h>
#include <linux/nodemask.h>
#include <linux/percpu.h>

/*
 * Register NUMA nodes and cpus in sysfs
 *
 * Ideally we could rely on CONFIG_GENERIC_CPU_DEVICES to register cpus.
 * Unfortunately this option registers cpus before topology_init() is called,
 * and crashes the system. Nodes have to be register before cpus.
 */

static DEFINE_PER_CPU(struct cpu, cpu_devices);

static int __init topology_init(void)
{
	int i, err = 0;

#ifdef CONFIG_NUMA
	for_each_online_node(i) {
		if ((err = register_one_node(i)))
			goto out;
	}
#endif

	for_each_present_cpu(i) {
		if ((err = register_cpu(&per_cpu(cpu_devices, i), i)))
			goto out;
	}

out:
	return err;
}

subsys_initcall(topology_init);
