#include <linux/ioport.h>
#include <linux/memblock.h>
#include <linux/mmzone.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/numa.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;

unsigned char tsar_ywidth;
unsigned char tsar_xwidth;

void __init memory_setup_nodes(void)
{
	struct memblock_region *reg;

	/* deduce the size of the grid from the last memblock */
	/* warning: in LETI system, the cluster IO is not part of this grid */
	tsar_xwidth = PA_TO_CLUSTERID_X(memblock_end_of_DRAM()) + 1;
	tsar_ywidth = PA_TO_CLUSTERID_Y(memblock_end_of_DRAM()) + 1;

	/* check enough nodes were compiled */
	BUG_ON((tsar_xwidth * tsar_ywidth) > MAX_NUMNODES);

	for_each_memblock(memory, reg) {
		unsigned char x = PA_TO_CLUSTERID_X(reg->base);
		unsigned char y = PA_TO_CLUSTERID_Y(reg->base);

		unsigned char nid = y * tsar_xwidth + x;

		/* check the memory block is inside the grid */
		BUG_ON((x >= tsar_xwidth) || (y >= tsar_ywidth));
		BUG_ON(nid >= MAX_NUMNODES);
		/* the node shouldn't already be online */
		WARN_ON(node_online(nid));

		/* associate the memory block with its corresponding node */
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

	unsigned char nid = y * tsar_xwidth + x;

	/* don't check the y coordinate to allow the IO cluster on LETI system
	 * to get a node number from this function */
	BUG_ON(x >= tsar_xwidth);

	return nid;
}

int of_node_to_nid(struct device_node *device)
{
	struct resource res;

	/* get the physical address of the device node */
	if (of_address_to_resource(device, 0, &res))
		return NUMA_NO_NODE;

	/* deduce the numa node id from it */
	return paddr_to_nid(res.start);
}
