#ifndef _ASM_TSAR_MMZONE_H
#define _ASM_TSAR_MMZONE_H

#include <linux/pfn.h>

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

	for_each_online_node(node) {
		if (pfn >= node_start_pfn(node) && pfn < node_end_pfn(node))
			break;
	}

	if (node == MAX_NUMNODES)
		return NUMA_NO_NODE;

	return node;
}

#else

/* fast version */
static inline unsigned int pfn_to_nid(unsigned long pfn)
{
	return paddr_to_nid(PFN_PHYS(pfn));
}

#endif

static inline int pfn_valid(unsigned long pfn)
{
	unsigned int nid = pfn_to_nid(pfn);

	if (nid < MAX_NUMNODES && NODE_DATA(nid)
			&& pfn >= node_start_pfn(nid)
			&& pfn < node_end_pfn(nid))
		return 1;
	return 0;
}

#endif

#endif /* _ASM_TSAR_MMZONE_H */
