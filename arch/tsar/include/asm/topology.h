#ifndef _ASM_TSAR_TOPOLOGY_H
#define _ASM_TSAR_TOPOLOGY_H

#ifdef CONFIG_NUMA

#include <asm/numa.h>

#define cpumask_of_node(node)		\
	((node) == NUMA_NO_NODE ?	\
	 cpu_all_mask :			\
	 &node_cpumask_map[node])

/* TSAR is clusterized in a flat manner. A node doesn't have a parent */
#define parent_node(node) (node)

#endif

#include <asm-generic/topology.h>

#endif /* _ASM_TSAR_TOPOLOGY_H */
