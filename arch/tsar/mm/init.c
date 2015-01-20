/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sizes.h>
#include <linux/sort.h>
#include <linux/swap.h>
#include <linux/types.h>

#include <asm/numa.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/setup.h>

/* the reference kernel page table, first object in bss section */
pgd_t swapper_pg_dir[PTRS_PER_PGD] __section(.bss..swapper_pg_dir);

/* always zero filled page */
unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)]
__page_aligned_bss;

/* make sure the vmalloc area will be 128MB minimum */
static void * __initdata vmalloc_min =
	(void *)(VMALLOC_END - SZ_128M - VMALLOC_OFFSET);

static unsigned long __initdata lowmem_limit = 0;

#ifndef CONFIG_NEED_MULTIPLE_NODES
static inline void *__init early_memory_setup_nodes(unsigned long limit)
{
	unsigned long limit_pfn = PFN_DOWN(limit);

	/* put the entire memory address space in node 0 */
	memblock_set_node(0, (phys_addr_t)ULLONG_MAX, 0);

	/* upper limit for memblock allocation:
	 * reserve the area instead of using memblock_set_current_limit() so we
	 * match the NUMA approach */
	memblock_reserve(limit, PFN_PHYS(max_pfn) - limit);

#ifndef CONFIG_HIGHMEM
	if (max_pfn > limit_pfn) {
		pr_warning("Warning: memory above %ldMB cannot be used"
				" (!CONFIG_HIGHMEM).\n",
				(unsigned long)(limit >> 20));
	}
#endif
	max_low_pfn = limit_pfn;

	/* return the high memory limit which determines the starting point of
	 * the vmalloc area */
	return __va_offset(limit);
}
static inline void __init memory_setup_nodes(void) {}
#endif

static void __init memory_init(void)
{
	/* complete init_mm */
	init_mm.start_code = (unsigned long) _stext;
	init_mm.end_code   = (unsigned long) _etext;
	init_mm.end_data   = (unsigned long) _edata;
	init_mm.brk	   = (unsigned long) _end;

	/* reserve the kernel text and data */
	memblock_reserve(__pa_offset((unsigned long)_stext), _end - _stext);

	/* minimum and maximum physical page numbers */
	min_low_pfn = PFN_UP(memblock_start_of_DRAM());
	max_pfn = PFN_DOWN(memblock_end_of_DRAM());

	/* the maximum amount of possible lowmem */
	lowmem_limit = min(__pa_offset((unsigned long)vmalloc_min & PAGE_MASK),
			PFN_PHYS(max_pfn));

	/* associate memory blocks with nodes
	 * determine lowmem mapping
	 * returns high_memory boundary */
	high_memory = early_memory_setup_nodes(lowmem_limit);

	/* reservation is completed - memblock internals can resize itself */
	memblock_allow_resize();
	memblock_dump_all();

	pr_debug("%s: min_low_pfn: %#lx\n", __func__, min_low_pfn);
	pr_debug("%s: max_low_pfn: %#lx\n", __func__, max_low_pfn);
	pr_debug("%s: max_pfn: %#lx\n", __func__, max_pfn);
}

static void __init prepare_page_table(void)
{
	/* This function is quite conservative. From what the reset sequence
	 * has done up to here, only the (small) identity mapping that was
	 * created at boot should be removed. */

	unsigned long vaddr;

	/* clear the mapping up to PAGE_OFFSET: the goal is to remove the
	 * identity mapping of the kernel image that was done in head.S, and
	 * that was necessary between MMU activation and the jump into the VA
	 * space */
	for (vaddr = 0; vaddr < PAGE_OFFSET; vaddr += PMD_SIZE)
		pmd_clear((pmd_t*)pgd_offset_kernel(vaddr));

	/* clear the mapping from the end of the kernel up to the almost end of
	 * the VA space (except the last section mapping to preserve the fixed
	 * ioremap area) */
	for (vaddr = ALIGN((unsigned long)_end, PMD_SIZE);
			vaddr < (FIXADDR_START & PMD_MASK);
			vaddr += PMD_SIZE)
		pmd_clear((pmd_t*)pgd_offset_kernel(vaddr));
}

static void __init map_pmd_section(pgd_t *pgd, unsigned long vaddr,
		phys_addr_t paddr)
{
	pmd_t *pmd = pmd_offset((pud_t*)pgd, vaddr);

	/* the low memory should be aligned on big pages */
	BUG_ON(((vaddr | paddr) & ~PMD_MASK) != 0);

	/* section mapping (PTE1) */
	set_pmd(pmd, __pmd(paddr >> PMD_SHIFT | __PMD_SECT));
}

static void __init map_lowmem(void)
{
	u64 i;
	unsigned int nid;
	phys_addr_t paddr, pend;

	/* map the lowmem areas:
	 * They have been computed earlier and are the only ones being "free".
	 * Highmem areas are reserved. */
	for_each_free_mem_range(i, MAX_NUMNODES, &paddr, &pend, &nid) {
		unsigned long vaddr, vend;
		pgd_t *pgd;

		/* for node0, align the start address to the next big page
		 * (because of the kernel reservation in memory_init) */
		if (!nid)
			paddr = ALIGN(paddr, PMD_SIZE);

		vaddr = (unsigned long)__va(paddr);
		vend = (unsigned long)__va(pend);
		pgd = pgd_offset_kernel(vaddr);
		do {
			unsigned long vnext;
			vnext = pgd_addr_end(vaddr, vend);

			map_pmd_section(pgd, vaddr, paddr);

			paddr += vnext - vaddr;
			vaddr = vnext;
			pgd++;
		} while (vaddr != vend);
	}
}

static size_t __init pgd_range_count(unsigned long start, unsigned long end)
{
	size_t count = 0;

	while (start != end) {
		count++;
		start = pgd_addr_end(start, end);
	};

	return count;
}

static void __init map_pmd_table(pgd_t *pgd, unsigned long vaddr, phys_addr_t paddr)
{
	pmd_t *pmd = pmd_offset((pud_t*)pgd, vaddr);

	/* table mapping (PTD) */
	set_pmd(pmd, __pmd(paddr >> PAGE_SHIFT | __PMD_TABLE));
}

static void __init pgd_range_init(unsigned long start, unsigned long end)
{
	size_t count = pgd_range_count(start, end);
	pgd_t *pgd;
	phys_addr_t pte = 0;

	if (unlikely(!count))
		return;

	/* allocate contiguous memory to hold pte pages */
	pte = memblock_alloc(count * PAGE_SIZE, PAGE_SIZE);
	BUG_ON(!pte);

	pgd = pgd_offset_kernel(start);

	while (start != end) {
		unsigned long next;
		next = pgd_addr_end(start, end);

		map_pmd_table(pgd, start, pte);

		/* clear pte page */
		clear_page(__va(pte));

		start = next;
		pgd++;
		pte += (PTRS_PER_PTE * sizeof(pte_t));
	};
}

static void __init fixmap_kmap_init(void)
{
	/* allocate second level page table(s) for the fixmap area */
	pgd_range_init(FIXADDR_START, FIXADDR_TOP);

#ifdef CONFIG_HIGHMEM
	/* allocate second level page table(s) for the pkmap area */
	pgd_range_init(PKMAP_BASE, PKMAP_BASE + PAGE_SIZE * LAST_PKMAP);

	/* initialize highmem/pkmap layer */
	kmap_init();
#endif

}

static void __init zones_size_init(void)
{
	unsigned int nid;

	setup_nr_node_ids();

	printk("Memory node ranges\n");

	for_each_online_node(nid) {
		unsigned long zones_size[MAX_NR_ZONES];
		unsigned long start_pfn, end_pfn;
		unsigned long local_start_pfn, local_end_pfn;

		/* reset the zones for each node */
		memset(zones_size, 0, sizeof(zones_size));

		/* XXX: we assume there is only one contiguous memory segment
		 * per node - it's too much of a pain to support multiple
		 * segments */
		get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);

		local_start_pfn = PFN_TO_LOCAL_PFN(start_pfn);
		local_end_pfn = PFN_TO_LOCAL_PFN(end_pfn);

		printk("  node %3d: [mem %#010llx-%#010llx]\n", nid,
				PFN_PHYS(start_pfn), PFN_PHYS(end_pfn) - 1);

		if (NUMA_HIGHMEM_NODE(nid)) {
#ifdef CONFIG_HIGHMEM
			/* the whole block is highmem */
			zones_size[ZONE_HIGHMEM] = local_end_pfn -
				local_start_pfn;
#endif
		} else {
#ifdef CONFIG_HIGHMEM
			unsigned long high_start_pfn, high_end_pfn;
#endif
			unsigned long low_start_pfn, low_end_pfn;

#ifdef CONFIG_HIGHMEM
			high_start_pfn = max(local_start_pfn, max_low_pfn);
			high_end_pfn = max(local_end_pfn, max_low_pfn);
			zones_size[ZONE_HIGHMEM] = high_end_pfn - high_start_pfn;
#endif
			low_start_pfn = min(local_start_pfn, max_low_pfn);
			low_end_pfn = min(local_end_pfn, max_low_pfn);
			zones_size[ZONE_NORMAL] = low_end_pfn - low_start_pfn;
		}

		free_area_init_node(nid, zones_size, start_pfn, NULL);

		if (node_present_pages(nid))
			node_set_state(nid, N_MEMORY);
		check_for_memory(NODE_DATA(nid), nid);
	}
}

void __init paging_init(void)
{
	/* complete memblock initialization, reserve kernel memory and compute
	 * the boundaries of low and high memory */
	memory_init();

	/* cleanup reference page table */
	prepare_page_table();

	/* map all the lowmem memory banks
	 *
	 * IMPORTANT: it's only after this fuction that we can finally allocate
	 * memory. Before it was impossible because the memory wasn't mapped!
	 */
	map_lowmem();

	/* finish configuring nodes */
	memory_setup_nodes();

	fixmap_kmap_init();

	/* zones size
	 * (can't do it before because it allocates memory and the page table
	 * is not ready until now) */
	zones_size_init();
}

static void __init free_highmem(void)
{
#ifdef CONFIG_HIGHMEM
	int i, nid;
	unsigned long start_pfn, end_pfn;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		unsigned long pfn;

		if (NUMA_HIGHMEM_NODE(nid)) {
			/* free the whole bank */
			pfn = start_pfn;
		} else if (PFN_TO_LOCAL_PFN(end_pfn) > max_low_pfn) {
			/* part of the bank is highmem */
			pfn = ALIGN(start_pfn + max_low_pfn, max_low_pfn);
		} else
			continue;

		for ( ; pfn < end_pfn; pfn++)
			free_highmem_page(pfn_to_page(pfn));
	}
#endif
}

static void __init free_lowmem(void)
{
	u64 i;
	phys_addr_t paddr, pend;

	reset_all_zones_managed_pages();

	/* lowmem has been put in free ranges */
	for_each_free_mem_range(i, MAX_NUMNODES, &paddr, &pend, NULL) {
		unsigned long start_pfn = PFN_UP(paddr);
		unsigned long end_pfn = PFN_DOWN(pend);
		int order;

		/* unfortunately, it is possible that for_each_free_mem_range()
		 * returns an invalid range... */
		if (start_pfn >= end_pfn)
			continue;

		totalram_pages += end_pfn - start_pfn;

		while (start_pfn < end_pfn) {
			order = min(MAX_ORDER - 1UL, __ffs(start_pfn));

			while (start_pfn + (1UL << order) > end_pfn)
				order --;

			__free_pages_bootmem(pfn_to_page(start_pfn), order);

			start_pfn += (1UL << order);
		}
	}
}

void __init mem_init(void)
{
#ifdef CONFIG_FLATMEM
	/* mem_map was allocated with nopanic */
	BUG_ON(!mem_map);
#endif

	/* release memory to the buddy allocator */
	free_lowmem();
	free_highmem();

	mem_init_print_info(NULL);

	pr_info("Virtual kernel memory layout:\n");
	pr_cont("    fixmap  : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			FIXADDR_START, FIXADDR_TOP, FIXADDR_SIZE >> 10);
#ifdef CONFIG_HIGHMEM
	pr_cont("    pkmap   : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			PKMAP_BASE, PKMAP_BASE + LAST_PKMAP * PAGE_SIZE,
			(LAST_PKMAP * PAGE_SIZE) >> 10);
#endif
	pr_cont("    vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
			VMALLOC_START, VMALLOC_END,
			(VMALLOC_END - VMALLOC_START) >> 20);
	pr_cont("    lowmem  : 0x%08lx - 0x%08lx   (%4ld MB) (cached)\n",
			(unsigned long)__va_offset(0),
			(unsigned long)high_memory,
			((unsigned long)high_memory -
			 (unsigned long)__va_offset(0)) >> 20);
	pr_cont("      .init : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			(unsigned long)&__init_begin, (unsigned long)&__init_end,
			((unsigned long)&__init_end -
			 (unsigned long)&__init_begin) >> 10);
	pr_cont("      .data : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			(unsigned long)&_etext, (unsigned long)&_edata,
			((unsigned long)&_edata - (unsigned long)&_etext) >> 10);
	pr_cont("      .text : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			(unsigned long)&_stext, (unsigned long)&_etext,
			((unsigned long)&_etext - (unsigned long)&_stext) >> 10);

	/*
	 * Check boundaries twice: Some fundamental inconsistencies can
	 * be detected at build time already.
	 */
#define high_memory (-128UL << 20)
	/* vmalloc area is at least 128MiB */
	BUILD_BUG_ON(VMALLOC_START >= VMALLOC_END);
#undef high_memory

	BUG_ON(VMALLOC_START >= VMALLOC_END);
	BUG_ON((unsigned long)high_memory > VMALLOC_START);
}

void __init_refok free_initmem(void)
{
	free_initmem_default(POISON_FREE_INITMEM);
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
	free_reserved_area((void *)start, (void *)end, POISON_FREE_INITMEM, "initrd");
}
#endif
