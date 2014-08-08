#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/node.h>
#include <linux/nodemask.h>
#include <linux/percpu.h>

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
