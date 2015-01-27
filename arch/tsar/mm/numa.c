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
#include <linux/slab.h>

#include <asm/numa.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/sections.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;

unsigned char numa_distance[MAX_NUMNODES][MAX_NUMNODES];

unsigned char tsar_ywidth;
unsigned char tsar_xwidth;

unsigned char node_lowmem_sz_log2;
unsigned char node_lowmem_sc_log2;

unsigned long node_lowmem_sz_mask;
unsigned char node_lowmem_sc_mask;

void *__init early_memory_setup_nodes(unsigned long lowmem_limit)
{
	struct memblock_region *reg;
	phys_addr_t memblock_size = 0;
	int i, nid;
	unsigned long start_pfn, end_pfn;

	unsigned long node_lowmem_size;

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

		pr_debug("Node %d of size %llu MB is online\n",
				nid, memblock_size >> 20);
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
	node_lowmem_sc_log2 = 0;
	node_lowmem_sz_log2 = ilog2(memblock_size);

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
				node_lowmem_sz_log2 -= 1;
			} while ((num_online_nodes() << node_lowmem_sz_log2) >
					lowmem_limit);
		} else {
			/* we reduce the number of lowmem mappable nodes until
			 * it fits */
			do {
				node_lowmem_sc_log2 += 1;
			} while (((num_online_nodes() >> node_lowmem_sc_log2) *
						min_node_size) > lowmem_limit);
		}
	}

	node_lowmem_size = (1 << node_lowmem_sz_log2);
	node_lowmem_sz_mask = node_lowmem_size - 1;
	node_lowmem_sc_mask = (1 << node_lowmem_sc_log2) - 1;

	pr_info("Limit low memory to %ld MB per node\n",
			node_lowmem_size >> 20);
	pr_info("Limit low memory to 1/%lu nodes\n",
			(1UL << node_lowmem_sc_log2));

	/* set the highmem as being reserved memory, in order to distinguish it
	 * from lowmem */
	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		phys_addr_t start = PFN_PHYS(start_pfn);
		phys_addr_t end = PFN_PHYS(end_pfn);

		if (NUMA_HIGHMEM_NODE(nid)) {
			/* the whole bank is highmem */
			memblock_reserve(start, end - start);
			pr_debug("Node %d is purely highmem\n", nid);
		} else if (PA_TO_LOCAL_ADDR(end) > node_lowmem_size) {
			/* part of the bank is highmem */
			start = ALIGN(start + node_lowmem_size, node_lowmem_size);
			memblock_reserve(start, end - start);
			pr_debug("Node %d is part highmem: [%#010llx-%#010llx]\n",
					nid, start, end);
		}
	}

	/* set max_low_pfn to what the first node can address */
	max_low_pfn = PFN_DOWN(node_lowmem_size);
	pr_debug("max_low_pfn = %lu\n", max_low_pfn);

	/* the highmem starts after the lowmem mapping of N/scale nodes */
	return NID_TO_LOWMEM_VADDR(num_online_nodes());
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

#ifdef CONFIG_KTEXT_REPLICATION
static phys_addr_t node_ktext_replication[MAX_NUMNODES];

unsigned char node_ktext_sc_log2;
unsigned char node_ktext_count;

static void __init init_ktext_replication(void)
{
	phys_addr_t ktext_start;
	phys_addr_t ktext_size;
	phys_addr_t ktext_replicat;

	unsigned long max_lowmem_size;
	unsigned char num_lowmem_nodes;

	unsigned char node_ktext_sc_mask;

	unsigned int nid;

	/* round values in big pages */
	ktext_start = round_down(__pa(_stext), PMD_SIZE);
	ktext_size = round_up(__pa(_etext) - ktext_start, PMD_SIZE);

	/* the maximum size we are willing to allocate for ktext replication:
	 * 6.25% of the total lowmem */
	max_lowmem_size = (unsigned long)NID_TO_LOWMEM_VADDR(num_online_nodes())
		>> 4;

	node_ktext_sc_log2 = 0;
	num_lowmem_nodes = num_online_nodes() >> node_lowmem_sc_log2;

	while (((num_lowmem_nodes >> node_ktext_sc_log2) * ktext_size) >
			max_lowmem_size) {
		/* we reduce the number of nodes which will contain ktext
		 * replication until it fits */
		node_ktext_sc_log2 += 1;
	}

	node_ktext_sc_mask = (1 << node_ktext_sc_log2) - 1;

#define NUMA_KTEXT_NODE(nid) !((nid) & node_ktext_sc_mask)

	/* by default the first replicate is the original copy */
	ktext_replicat = ktext_start;
	node_ktext_count = 1;

	/* allocate ktext replication and make node associations */
	for_each_online_node(nid) {
		if (nid && NUMA_KTEXT_NODE(nid)) {
			node_ktext_count++;
			/* replicate the ktext in this node (except for the
			 * boot node) */
			ktext_replicat = memblock_alloc_nid(ktext_size,
					PMD_SIZE, nid);
			/* memcopy the kernel */
			memcpy(__va(ktext_replicat), __va(ktext_start),
					ktext_size);
		}
		node_ktext_replication[nid] = ktext_replicat;
	}

	pr_info("Kernel text and rodata are replicated %d times\n",
			node_ktext_count);
}

void numa_ktext_patch_pgd(pgd_t *pgd, unsigned int nid)
{
	unsigned char count = 0;
	unsigned long vaddr = (unsigned long)_stext & PMD_MASK;

	/* scan the kernel mapping */
	for (; vaddr < ALIGN((unsigned long)_etext, PMD_SIZE);
			vaddr += PMD_SIZE, count++) {
		/* get current mapping */
		pmd_t *pmd = (pmd_t*)(pgd + pgd_index(vaddr));
		unsigned long pmd_val = pmd_val(*pmd);

		/* mask out current PPN */
		pmd_val &= _PAGE_PPN1_MASK;

		/* inject the new PPN */
		pmd_val |= ((node_ktext_replication[nid] >> PMD_SHIFT) + count);

		/* setup new mapping */
		set_pmd(pmd, __pmd(pmd_val));
	}
}

static pgd_t **numa_ktext_pgd;

pgd_t *numa_ktext_get_pgd(unsigned int nid)
{
	unsigned char ktext_nid = NID_TO_KTEXT_NID(nid);

	if (!numa_ktext_pgd) {
		/* first time we call this function, alloc the array of pgd */
		numa_ktext_pgd = kzalloc(node_ktext_count * sizeof(pgd_t*),
				GFP_KERNEL);
		numa_ktext_pgd[0] = swapper_pg_dir;
	}

	if (!numa_ktext_pgd[ktext_nid]) {
		/* get a patched copy of swapper_pg_dir */
		numa_ktext_pgd[ktext_nid] = __pgd_alloc_knid(ktext_nid);
	}

	return numa_ktext_pgd[ktext_nid];
}

#endif

void __init memory_setup_nodes(void)
{
	/* alloc pgdat structure for each node */
	alloc_data_nodes();

	/* compute node distances */
	init_node_distance_table();

#ifdef CONFIG_KTEXT_REPLICATION
	/* kernel text and rodata replication */
	init_ktext_replication();
#endif
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

/*
 * Setup per-cpu memory
 */

unsigned long __per_cpu_offset[NR_CPUS] __read_mostly;

static void *__init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align)
{
	phys_addr_t ptr;
	int node = early_cpu_to_node(cpu);

	ptr = memblock_alloc_nid(size, align, NID_TO_LOWMEM_NID(node));
	return __va(ptr);
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
	memblock_free(__pa(ptr), size);
}

static void __init pcpu_fc_populate_pte(unsigned long addr)
{
	pmd_t *pmd;
	pte_t *pte;

	/* check the address is part of the vmalloc area */
	if (addr < VMALLOC_START || addr >= VMALLOC_END)
		panic("PCPU add %#lx is outside of vmalloc range %#lx..%#lx\n",
				addr, VMALLOC_START, VMALLOC_END);

	pmd = pmd_offset(pud_offset(pgd_offset_kernel(addr), addr), addr);
	if (!pmd_present(*pmd)) {
		pte = __va(memblock_alloc(PAGE_SIZE, PAGE_SIZE));
		memset(pte, 0, PAGE_SIZE);
		pmd_populate_kernel(&init_mm, pmd, pte);
	}
}

void __init setup_per_cpu_areas(void)
{
	unsigned long delta;
	unsigned int cpu;
	int rc;

	rc = pcpu_page_first_chunk(0, pcpu_fc_alloc, pcpu_fc_free,
			pcpu_fc_populate_pte);
	if (rc < 0)
		panic("Failed to initialized percpu area (err=%d)\n", rc);

	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu)
		__per_cpu_offset[cpu] = delta + pcpu_unit_offsets[cpu];
}
