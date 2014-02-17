/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>

#include <asm/fixmap.h>

extern int mem_init_done;

/*
 * ioremap_fixed API
 */

struct ioremap_fixed_map {
	void __iomem *addr;
	unsigned long size;
	unsigned long fixmap_addr;
};

/* array of fixed mappings */
static struct ioremap_fixed_map ioremap_fixed_map[FIX_N_IOREMAPS];

/* page table dedicated to cover the fixed mappings area */
static pte_t fixmap_page_table[PTRS_PER_PTE] __page_aligned_bss;

static void set_pte_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t prot)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset_kernel(vaddr);
	if (pgd_none(*pgd)) {
		pgd_ERROR(*pgd);
		return;
	}

	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {
		pud_ERROR(*pud);
		return;
	}

	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		pmd_ERROR(*pmd);
		return;
	}

	pte = pte_offset_kernel(pmd, vaddr);

	set_pte(pte, pfn_pte(pfn, prot));

	/* no need to flush the TLB, we're cc */
}

void __set_fixmap(enum fixed_addresses idx, phys_addr_t paddr, pgprot_t prot)
{
	unsigned long vaddr = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses)
		BUG();


	set_pte_pfn(vaddr, paddr >> PAGE_SHIFT, prot);
}

/* initialize the fixed mapping area:
 * - prepare the array of fixed mappings
 * - populate swapper_pg_dir with a the fixmap page table
 */
void __init ioremap_fixed_early_init(void)
{
	struct ioremap_fixed_map *map;
	int i;
	unsigned long vfix_ioremap_begin, vfix_ioremap_end;
	pmd_t *pmd;

	for (i = 0; i < FIX_N_IOREMAPS; i++) {
		map = &ioremap_fixed_map[i];
		map->fixmap_addr = __fix_to_virt(FIX_IOREMAP_BEGIN + i);
	}

	/* attach the proper page table to swapper_pg_dir */
	vfix_ioremap_begin = fix_to_virt(FIX_IOREMAP_BEGIN);
	pmd = pmd_offset(pud_offset(pgd_offset_kernel(vfix_ioremap_begin),
				vfix_ioremap_begin), vfix_ioremap_begin);
	set_pmd(pmd, __pmd(__pa(fixmap_page_table) >> PAGE_SHIFT | __PMD_TABLE));

	/* check that the fixed mappings area does not span multiple pmds */
	vfix_ioremap_end = fix_to_virt(FIX_IOREMAP_END);
	if (pmd != pmd_offset(pud_offset(pgd_offset_kernel(vfix_ioremap_end),
					vfix_ioremap_end), vfix_ioremap_end))
	{
		WARN_ON(1);
		pr_warning("FIX_IOREMAP area spans multiple pmds\n");
	}
}

/* ioremap in the fixed mappings area */
static void __init __iomem *ioremap_fixed(phys_addr_t paddr, unsigned long size, pgprot_t prot)
{
	struct ioremap_fixed_map *map;
	unsigned long offset;
	unsigned int nrpages;
	int i, slot;
	enum fixed_addresses idx;

	/* get mapping page-aligned */
	offset = paddr & ~PAGE_MASK;
	paddr &= PAGE_MASK;
	size = PAGE_ALIGN(paddr + size) - paddr;

	/* find a free fixed-mapping slot */
	slot = -1;
	for (i = 0; i < FIX_N_IOREMAPS; i++) {
		map = &ioremap_fixed_map[i];
		if (!map->addr) {
			map->size = size;
			slot = i;
			break;
		}
	}

	if (slot < 0)
		return NULL;

	/* check that the requested mapping fits */
	nrpages = size >> PAGE_SHIFT;
	if (nrpages > FIX_N_IOREMAPS)
		return NULL;

	/* make the fixed mapping */
	idx = FIX_IOREMAP_BEGIN + slot;
	while (nrpages) {
		__set_fixmap(idx, paddr, prot);
		paddr += PAGE_SIZE;
		idx++;
		nrpages--;
	}

	map->addr = (void __iomem *)(map->fixmap_addr + offset);
	return map->addr;
}

/* iounmap in the fixed mappings area */
static int iounmap_fixed(void __iomem *addr)
{
	struct ioremap_fixed_map *map;
	unsigned int nrpages;
	int i, slot;
	enum fixed_addresses idx;

	/* find the corresponding slot */
	slot = -1;
	for (i = 0; i < FIX_N_IOREMAPS; i++) {
		map = &ioremap_fixed_map[i];
		if (map->addr == addr) {
			slot = i;
			break;
		}
	}

	/* we don't seem to match */
	if (slot < 0)
		return -EINVAL;

	nrpages = map->size >> PAGE_SHIFT;

	idx = FIX_IOREMAP_BEGIN + slot + nrpages - 1;
	while (nrpages > 0) {
		__clear_fixmap(idx);
		--idx;
		--nrpages;
	}

	map->size = 0;
	map->addr = NULL;

	return 0;
}

/*
 * ioremap API
 */
void __iomem * __init_refok
__ioremap(phys_addr_t paddr, unsigned long size, pgprot_t prot)
{
	struct vm_struct *area;
	unsigned long offset, last_paddr, vaddr;

	/* Don't allow wraparound or zero size */
	last_paddr = paddr + size - 1;
	if (!size || last_paddr < paddr)
		return NULL;

	/*
	 * If we can't yet use the regular approach, go the fixmap route.
	 */
	if (!mem_init_done)
		return ioremap_fixed(paddr, size, prot);

	/*
	 * Mappings have to be page-aligned
	 */
	offset = paddr & ~PAGE_MASK;
	paddr &= PAGE_MASK;
	size = PAGE_ALIGN(paddr + size) - paddr;

	/*
	 * Ok, go for it..
	 */
	area = get_vm_area(size, VM_IOREMAP);
	if (!area)
		return NULL;
	area->phys_addr = paddr;
	vaddr = (unsigned long)area->addr;

	if (ioremap_page_range(vaddr, vaddr + size, paddr, prot)) {
		vunmap((void*)vaddr);
		return NULL;
	}

	return (void __iomem *)(vaddr + offset);
}

void iounmap(void __iomem *addr)
{
	void *vaddr = (void*)((unsigned long)addr & PAGE_MASK);

	/* there's no VMA if it's from an early fixed mapping */
	if (iounmap_fixed(addr) == 0)
		return;

	vunmap(vaddr);
}
