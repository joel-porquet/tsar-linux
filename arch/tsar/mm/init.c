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

#include <asm/meminfo.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/sections.h>

/* the reference kernel page table */
pgd_t swapper_pg_dir[PTRS_PER_PGD] __section(.bss..swapper_pg_dir);

/* always null filled page */
unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)]  __page_aligned_bss;

unsigned int mem_init_done = 0;

static int __init meminfo_cmp(const void *_a, const void *_b)
{
	const struct membank *a = _a, *b = _b;
	long cmp = membank_phys_start(a) - membank_phys_start(b);
	return cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
}

/* make sure the vmalloc area will be 128MB minimum */
static void * vmalloc_min __initdata =
	(void *)(VMALLOC_END - SZ_128M - VMALLOC_OFFSET);
static phys_addr_t lowmem_limit __initdata = 0;

void __init tsar_memory_init(void)
{
	int i;

	/* complete init_mm */
	init_mm.start_code = (unsigned long) _stext;
	init_mm.end_code   = (unsigned long) _etext;
	init_mm.end_data   = (unsigned long) _edata;
	init_mm.brk	   = (unsigned long) _end;

	/* order the memory banks by starting addresses */
	sort(&meminfo.bank, meminfo.nr_banks, sizeof(meminfo.bank[0]), meminfo_cmp, NULL);

	/* TODO: deal with HIGHMEM */

	/* debug the memblock API */
	//memblock_debug = 1;

	/* add them to the memblock subsystem */
	for_each_membank(i, &meminfo) {
		struct membank *bank = &meminfo.bank[i];
		memblock_add(membank_phys_start(bank), membank_phys_size(bank));
	}

	/* register the kernel text and data */
	memblock_reserve(__pa(_stext), _end - _stext);

	/* reservation is completed - memblock internals can resize itself */
	memblock_allow_resize();
	memblock_dump_all();

	/* determine the characteristics of lowmem and highmem areas */
	min_low_pfn = PFN_UP(memblock_start_of_DRAM());

	/* maximum page frame number in the system */
	max_pfn = PFN_DOWN(memblock_end_of_DRAM());

	lowmem_limit = min((phys_addr_t)__pa(vmalloc_min - 1) + 1,
			(memblock_end_of_DRAM() - 1) + 1);

	/* tell memblock the upper limit for allocation */
	memblock_set_current_limit(lowmem_limit - 1);

	/* if amount of RAM is superior to lowmem_limit */
	if (max_pfn > PFN_DOWN(lowmem_limit)) {
		pr_warning("Warning, only %ldMB will be used.\n",
				(unsigned long)lowmem_limit >> 20);
		max_low_pfn = PFN_DOWN(lowmem_limit);
	} else {
		max_low_pfn = max_pfn;
	}

	pr_debug("%s: min_low_pfn: %#lx\n", __func__, min_low_pfn);
	pr_debug("%s: max_low_pfn: %#lx\n", __func__, max_low_pfn);
	pr_debug("%s: max_pfn: %#lx\n", __func__, max_pfn);

	/* TODO: let's not deal with sparse_init for now */
}

static void __init zones_size_init(void)
{
	unsigned long zones_size[MAX_NR_ZONES];

	/* zones setup */
	memset(zones_size, 0, sizeof(zones_size));
	zones_size[ZONE_NORMAL] = (max_low_pfn - min_low_pfn);
	//zones_size[ZONE_HIGHMEM] = (max_pfn - min_low_pfn);

	/* FIXME: we assume FLATMEM with no hole for now */
	free_area_init_node(0, zones_size, min_low_pfn, NULL);

	// we could put the following if we consider the PA space begins at
	// __pa(PAGE_OFFSET)
	//free_area_init(zones_size);
}

static void __init prepare_page_table(void)
{
	/* This function is being very conservative. From what we know of the
	 * reset sequence up to here, we'd just need to remove the (small)
	 * identity mapping that was created at boot */

	unsigned long vaddr;
	phys_addr_t end;

	/* clear the mapping up to PAGE_OFFSET: the goal is to remove the
	 * identity mapping of the kernel image that was done in head.S, and
	 * that was necessary between MMU activation and the jump into the VA
	 * space */
	for (vaddr = 0; vaddr < PAGE_OFFSET; vaddr += PMD_SIZE)
		pmd_clear((pmd_t*)pgd_offset_kernel(vaddr));

	/* I guess here that we assume end is aligned with the size of a
	 * section, so we're not going to remove a possible legit mapping at the end of
	 * the memory bank. Also we can assume, the kernel image is way smaller
	 * than the first memory bank anyway */
	end = memblock.memory.regions[0].base + memblock.memory.regions[0].size;
	if (end >= lowmem_limit)
		end = lowmem_limit;

	/* clear the mapping from the end of the first block of lowmem up to
	 * the almost end of the VA space (except the last section mapping to
	 * preserve the fixed ioremap area) */
	for (vaddr = (unsigned long)__va(end);
			vaddr < (FIXADDR_START & PMD_MASK);
			vaddr += PMD_SIZE)
		pmd_clear((pmd_t*)pgd_offset_kernel(vaddr));
}

static void * __init early_alloc(unsigned long size)
{
	void *ptr = __va(memblock_alloc(size, size));
	memset(ptr, 0, size);
	return ptr;
}

static pte_t * __init early_pte_alloc(pmd_t *pmd, unsigned long vaddr)
{
	if (pmd_none(*pmd)) {
		pte_t *pte = (pte_t*)early_alloc(PTRS_PER_PTE * sizeof(pte_t));
		pmd_populate_kernel(&init_mm, pmd, pte);
	}
	BUG_ON(pmd_bad(*pmd));
	return pte_offset_kernel(pmd, vaddr);
}

static void __init alloc_init_pte(pmd_t *pmd, unsigned long vaddr, unsigned long vend, phys_addr_t paddr)
{
	pte_t *pte = early_pte_alloc(pmd, vaddr);
	do {
		set_pte(pte, pfn_pte(paddr >> PAGE_SHIFT, PAGE_KERNEL));
		paddr += PAGE_SIZE;
		vaddr += PAGE_SIZE;
		pte++;
	} while (vaddr != vend);
}

static void __init alloc_init_pmd(pgd_t *pgd, unsigned long vaddr, unsigned long vend, phys_addr_t paddr)
{
	pmd_t *pmd = pmd_offset((pud_t*)pgd, vaddr);

	if (((vaddr | vend | paddr) & ~PMD_MASK) == 0)
	{
		/* try a section mapping (PTE1), in case everything is aligned */
		set_pmd(pmd, __pmd(paddr >> PMD_SHIFT | pgprot_val(PMD_SECT)));
	} else {
		/* otherwise we need a second level (PTE2) */
		alloc_init_pte(pmd, vaddr, vend, paddr);
	}
}

static void __init map_lowmem(void)
{
	struct memblock_region *reg;

	/* map all the lowmem memory banks. */
	for_each_memblock(memory, reg) {
		phys_addr_t paddr = reg->base;
		phys_addr_t pend = paddr + reg->size;
		unsigned long vaddr, vend;
		pgd_t *pgd;

		/* do not map beyond lowmem */
		if (pend > lowmem_limit)
			pend = lowmem_limit;
		if (paddr >= pend)
			break;

		vaddr = (unsigned long)__va(paddr);
		vend = (unsigned long)__va(pend);
		pgd = pgd_offset_kernel(vaddr);
		do {
			unsigned long vnext;
			vnext = pgd_addr_end(vaddr, vend);

			alloc_init_pmd(pgd, vaddr, vnext, paddr);

			paddr += vnext - vaddr;
			vaddr = vnext;
			pgd++;
		} while (vaddr != vend);
	}
}

void __init paging_init(void)
{
	pr_debug("paging init...\n");

	/* prepare page table:
	 * - clean up to PAGE_OFFSET
	 * - and from the end of the first memory bank up to VMALLOC_START */
	prepare_page_table();

	/* map all the lowmem memory banks */
	map_lowmem();

	/* zones size
	 * (can't do it before because it allocates memory and the page table
	 * is not ready until now) */
	zones_size_init();
}

void __init mem_init(void)
{
	int codesize, datasize, bsssize, initsize, reservedpages;
	unsigned long pfn;

#ifdef CONFIG_FLATMEM
	BUG_ON(!mem_map);
#endif

	max_mapnr = num_physpages = max_low_pfn;

	high_memory = (void*)__va(max_low_pfn * PAGE_SIZE);

	/* no need to clear out the zero-page, since we allocated it in the bss
	 * section */

	/* this will put all low memory onto the freelists */
	totalram_pages += free_all_bootmem();

	reservedpages = 0;
	for (pfn = 0; pfn < max_low_pfn; pfn++) {
		/* only count reserved RAM pages */
		if (PageReserved(pfn_to_page(pfn)))
			reservedpages++;
	}

	codesize = (unsigned long)_etext - (unsigned long)_stext;
	datasize = (unsigned long)_edata - (unsigned long)_sdata;
	bsssize = (unsigned long)__bss_stop - (unsigned long)__bss_start;
	initsize = (unsigned long)__init_end - (unsigned long)__init_begin;

	pr_info("Memory: %luk/%luk available (%dk kernel code, "
			"%dk reserved, %dk data, %dk bss, %dk init, "
			"%ldk highmem)\n",
			nr_free_pages() << (PAGE_SHIFT - 10),
			num_physpages << (PAGE_SHIFT - 10),
			codesize >> 10,
			reservedpages << (PAGE_SHIFT - 10),
			datasize >> 10,
			initsize >> 10,
			bsssize >> 10,
			0UL << (PAGE_SHIFT - 10));

	pr_info("Virtual kernel memory layout:\n");
	pr_cont("    fixmap  : 0x%08lx - 0x%08lx   (%4ld kB)\n",
			FIXADDR_START, FIXADDR_TOP, FIXADDR_SIZE >> 10);
	pr_cont("    vmalloc : 0x%08lx - 0x%08lx   (%4ld MB)\n",
			VMALLOC_START, VMALLOC_END,
			(VMALLOC_END - VMALLOC_START) >> 20);
	pr_cont("    lowmem  : 0x%08lx - 0x%08lx   (%4ld MB) (cached)\n",
			(unsigned long)__va(0), (unsigned long)high_memory,
			((unsigned long)high_memory - (unsigned long)__va(0))
			>> 20);
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
#define __FIXADDR_TOP (-PAGE_SIZE)
#define high_memory (-128UL << 20)
	/* vmalloc area is at least 128MiB */
	BUILD_BUG_ON(VMALLOC_START >= VMALLOC_END);
#undef high_memory
#undef __FIXADDR_TOP

	BUG_ON(VMALLOC_START >= VMALLOC_END);
	BUG_ON((unsigned long)high_memory > VMALLOC_START);

	/* finally, we're done with memory initialization */
	mem_init_done = 1;
}

void __init_refok free_initmem(void)
{
	free_initmem_default(POISON_FREE_INITMEM);
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init free_initrd_mem(unsigned long start, unsigned long end)
{
	free_reserved_area(start, end, POISON_FREE_INITMEM, "initrd");
}
#endif
