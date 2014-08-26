#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/node.h>
#include <linux/nodemask.h>
#include <linux/percpu.h>

#ifdef CONFIG_NUMA

/*
 * Register NUMA nodes in sysfs
 *
 * Note: we don't have to register cpus, they're already getting registered by
 * GENERIC_CPU_DEVICES during init.
 */

static int __init topology_init(void)
{
	int i, err = 0;

	for_each_online_node(i) {
		if ((err = register_one_node(i)))
			goto out;
	}

out:
	return err;
}

subsys_initcall(topology_init);

#endif
