/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/mmzone.h>

#include <asm/numa.h>
#include <asm/smp_map.h>

unsigned char cpu_node_map[NR_CPUS];
cpumask_t node_cpumask_map[MAX_NUMNODES];

void __init cpu_setup_nodes(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		unsigned long hwcpuid = cpu_logical_map(cpu);

		unsigned char x = HWCPUID_TO_CLUSTERID_X(hwcpuid);
		unsigned char y = HWCPUID_TO_CLUSTERID_Y(hwcpuid);

		unsigned char nid = y * tsar_xwidth + x;

		/* check the cpu is inside the grid */
		BUG_ON((x >= tsar_xwidth) || (y >= tsar_ywidth));
		BUG_ON(nid >= MAX_NUMNODES);
		/* check the corresponding node is online */
		BUG_ON(!node_online(nid));

		cpu_node_map[cpu] = nid;
		cpumask_set_cpu(cpu, &node_cpumask_map[nid]);
	}
}

