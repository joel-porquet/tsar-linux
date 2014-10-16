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

/*
 * node distance
 */
extern unsigned char numa_distance[MAX_NUMNODES][MAX_NUMNODES];
#define node_distance(from,to) (numa_distance[(from)][(to)])

#endif

#include <asm-generic/topology.h>

#endif /* _ASM_TSAR_TOPOLOGY_H */
