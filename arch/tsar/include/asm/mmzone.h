#ifndef _ASM_TSAR_MMZONE_H
#define _ASM_TSAR_MMZONE_H

#include <asm/numa.h>

/*
 * one pglist_data per node
 */

extern struct pglist_data *node_data[MAX_NUMNODES];

#define NODE_DATA(nid) (node_data[nid])


/*
 * node memory support
 */

#ifdef CONFIG_NUMA
#if 0

/* slow version */
static inline int pfn_to_nid(unsigned long pfn)
{
	int node;

	for (node = 0; node < MAX_NUMNODES; node++) {
		if (pfn >= node_start_pfn(node) &&
				pfn < node_end_pfn(node))
			break;
	}

	if (node == MAX_NUMNODES)
		return NUMA_NO_NODE;

	return node;
}

#else

/* fast version */
#define pfn_to_nid(pfn)	paddr_to_nid((pfn) << PAGE_SHIFT)

#endif

static inline int pfn_valid(int pfn)
{
	return (pfn_to_nid(pfn) != NUMA_NO_NODE);
}

#endif

#endif /* _ASM_TSAR_MMZONE_H */
