/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/ioport.h>
#include <linux/memblock.h>
#include <linux/mmzone.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <asm/numa.h>
#include <asm/sections.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;

unsigned char numa_distance[MAX_NUMNODES][MAX_NUMNODES];

unsigned char tsar_ywidth;
unsigned char tsar_xwidth;

unsigned long node_lowmem_size;
unsigned char node_lowmem_scale;

unsigned char node_lowmem_sz_log2;
unsigned char node_lowmem_sc_log2;

void *__init early_memory_setup_nodes(unsigned long lowmem_limit)
{
	struct memblock_region *reg;
	phys_addr_t memblock_size = 0;
	int i, nid;
	unsigned long start_pfn, end_pfn;

	/* deduce the size of the grid from the last memblock */
	/* warning: in LETI system, the node IO is not part of this grid */
	tsar_xwidth = PA_TO_CLUSTERID_X(memblock_end_of_DRAM()) + 1;
	tsar_ywidth = PA_TO_CLUSTERID_Y(memblock_end_of_DRAM()) + 1;

	/* check enough nodes were compiled */
	BUG_ON((tsar_xwidth * tsar_ywidth) > MAX_NUMNODES);

	for_each_memblock(memory, reg) {
		unsigned char x = PA_TO_CLUSTERID_X(reg->base);
		unsigned char y = PA_TO_CLUSTERID_Y(reg->base);

		nid = y * tsar_xwidth + x;

		/* check the memory block is inside the grid */
		BUG_ON((x >= tsar_xwidth) || (y >= tsar_ywidth));
		BUG_ON(nid >= MAX_NUMNODES);

		/* the node shouldn't already be online (except node #0 which
		 * is online by default) */
		BUG_ON(nid && node_online(nid));

		/* check all the memory blocks have the same size (for now we
		 * bug, but maybe we could trim instead) */
		memblock_size = (memblock_size) ? memblock_size : reg->size;
		BUG_ON(reg->size != memblock_size);

		/* check the memory block's local address is 0 and its size is
		 * a power of two (it's MANDATORY for facilitating the lowmem
		 * mapping computation) */
		BUG_ON(PA_TO_LOCAL_ADDR(reg->base));
		BUG_ON(!is_power_of_2(reg->size));

		/* associate the memory block with its corresponding node */
		memblock_set_region_node(reg, nid);

		node_set_online(nid);
	}

	/* cap physical memory:
	 * Each physical memory page requires a "struct page" object (~32
	 * bytes) which must be mapped in lowmem. From a certain amount of
	 * physical memory the footprint of mem_map begins to be too big to be
	 * mapped in the ~860Mb of available lowmem. */
	/* XXX: instead we bug for now if the physical memory exceeds the
	 * arbitrarily chosen value of 32Gb (which means 8 millions 4kb pages,
	 * thus a mem_map of at least ~256Mb) */
	BUG_ON(!!(memblock_phys_mem_size() >> 35));

	/* determine lowmem mapping:
	 * In TSAR, there is one memory segment per node and the address space
	 * of each node is prefixed with the node coordinates: X (4bits) |Y
	 * (4bits) | LOCAL_ADDR_SPACE (32bits). By default, with this
	 * configuration, Linux only maps the first node (starting at @0) and
	 * all the nodes above are part of highmem (because of their MSB).
	 * Here we try to find the best strategy that maps as much physical
	 * memory as possible in lowmem.
	 */

	/* by default we assume the amount of physical memory can entirely fit
	 * in lowmem: in this case we stack all the nodes in lowmem */
	node_lowmem_scale = 1;
	node_lowmem_size = memblock_size;

	if (memblock_phys_mem_size() > lowmem_limit) {
		/* minimum size we want to map in lowmem for each node (at
		 * least the size of kernel or the max zone for the buddy
		 * allocator) */
		unsigned long min_node_size = max(
				roundup_pow_of_two((unsigned long)(_end - _stext)),
				1UL << (MAX_ORDER + PAGE_SHIFT));

		if (num_online_nodes() * min_node_size <= lowmem_limit) {
			/* we reduce the amount of lowmem mappable memory in
			 * each node until it fits */
			do {
				node_lowmem_size >>= 1;
			} while ((num_online_nodes() * node_lowmem_size) >
					lowmem_limit);
		} else {
			/* we reduce the number of lowmem mappable nodes until
			 * it fits */
			do {
				node_lowmem_scale <<= 1;
			} while (((num_online_nodes() / node_lowmem_scale) *
						min_node_size) > lowmem_limit);
		}
	}

	node_lowmem_sz_log2 = ilog2(node_lowmem_size);
	node_lowmem_sc_log2 = ilog2(node_lowmem_scale);

	/* set the highmem as being reserved memory, in order to distinguish it
	 * from lowmem */
	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		phys_addr_t start = PFN_PHYS(start_pfn);
		phys_addr_t end = PFN_PHYS(end_pfn);

		if (NUMA_HIGHMEM_NODE(nid)) {
			/* the whole bank is highmem */
			memblock_reserve(start, end);
		} else if (PA_TO_LOCAL_ADDR(end) > node_lowmem_size) {
			/* part of the bank is highmem */
			memblock_reserve(start + node_lowmem_size, end);
		}
	}

	/* set max_low_pfn to what the first node can address */
	max_low_pfn = PFN_DOWN(node_lowmem_size);

	/* the highmem starts after the lowmem mapping of N/scale nodes */
	return __va_offset(((num_online_nodes() >> node_lowmem_sc_log2) <<
				node_lowmem_sz_log2));
}

static void __init alloc_data_nodes(void)
{
	unsigned int nid;

	for_each_online_node(nid) {
		phys_addr_t pgdat_paddr;
		const size_t pgdat_size = sizeof(struct pglist_data);

		BUG_ON(NODE_DATA(nid));

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
	}
}

static void __init init_node_distance_table(void)
{
	unsigned char nid_from, nid_to;

	/* initialize the table (-1 means no possible connection) */
	memset(numa_distance, -1, MAX_NUMNODES * MAX_NUMNODES);

	for_each_online_node(nid_from) {
		unsigned char x_from, y_from;

		y_from = nid_from / tsar_xwidth;
		x_from = nid_from - y_from * tsar_xwidth;

		for_each_online_node(nid_to) {
			unsigned char x_to, y_to;

			y_to = nid_to / tsar_xwidth;
			x_to = nid_to - y_to * tsar_xwidth;

			/* the distance between two nodes is at minimum
			 * LOCAL_DISTANCE, plus the manhattan distance between
			 * them multiplied by the LOCAL_DISTANCE (basically we
			 * add one LOCAL_DISTANCE unit for each hop of the 2-D
			 * mesh) */
			numa_distance[nid_from][nid_to] = LOCAL_DISTANCE +
				(abs(x_from - x_to) + abs(y_from - y_to)) * LOCAL_DISTANCE;
		}
	}
}

void __init memory_setup_nodes(void)
{
	/* alloc pgdat structure for each node */
	alloc_data_nodes();

	/* compute node distances */
	init_node_distance_table();
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
