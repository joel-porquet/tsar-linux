#include <linux/memblock.h>
#include <linux/mmzone.h>

#include <asm/numa.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;

unsigned char tsar_ywidth;
unsigned char tsar_xwidth;

void __init memory_setup_nodes(void)
{
	struct memblock_region *reg;

	/* get what is supposed to be the last cluster and deduce the size of
	 * the grid */
	tsar_xwidth = PA_TO_CLUSTERID_X(memblock_end_of_DRAM());
	tsar_ywidth = PA_TO_CLUSTERID_Y(memblock_end_of_DRAM());

	/* do we have enough nodes? */
	BUG_ON((tsar_xwidth * tsar_ywidth) >= MAX_NUMNODES);

	for_each_memblock(memory, reg) {
		unsigned char x = PA_TO_CLUSTERID_X(reg->base);
		unsigned char y = PA_TO_CLUSTERID_Y(reg->base);

		unsigned char nid = x * tsar_ywidth + y;

		/* check the memory block is inside the grid */
		BUG_ON((x > tsar_xwidth) || (y > tsar_ywidth));
		BUG_ON(nid >= MAX_NUMNODES);

		/* associate the memory block with a node */
		memblock_set_region_node(reg, nid);

		/* allocate node data */
		if (!NODE_DATA(nid)) {
			phys_addr_t pgdat_paddr;
			const size_t pgdat_size = sizeof(struct pglist_data);

			/* try to allocate the structure locally */
			pgdat_paddr = memblock_alloc_nid(pgdat_size,
					SMP_CACHE_BYTES,
					nid);

			if (!pgdat_paddr) {
				/* or globally */
				pgdat_paddr = memblock_alloc(pgdat_size,
						SMP_CACHE_BYTES);
				BUG_ON(!pgdat_paddr);
			}

			/* assign and clear */
			NODE_DATA(nid) = __va(pgdat_paddr);
			memset(NODE_DATA(nid), 0, pgdat_size);

			node_set_online(nid);
		}
	}
}

int paddr_to_nid(phys_addr_t paddr)
{
	unsigned char x = PA_TO_CLUSTERID_X(paddr);
	unsigned char y = PA_TO_CLUSTERID_Y(paddr);

	unsigned char nid = x * tsar_ywidth + y;

	if ((x > tsar_xwidth) || (y > tsar_ywidth))
		return NUMA_NO_NODE;

	if (nid >= MAX_NUMNODES)
		return NUMA_NO_NODE;

	return nid;
}
