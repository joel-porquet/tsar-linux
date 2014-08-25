/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/highmem.h>

/*
 * PKMAP infrastructure
 *
 * Mostly handled by linux core, we just provide the frontend functions:
 * kmap() and kunmap().
 *
 * All that Linux needs is a pointer to the consecutive set of PTEs
 * (pkmap_page_table) which cover the entire PKMAP area. The pointer is set in
 * kmap_init() below.
 */

extern void *kmap_high(struct page *page);
extern void kunmap_high(struct page *page);

void *kmap(struct page *page)
{
	might_sleep();
	if (!PageHighMem(page))
		return page_address(page);
	return kmap_high(page);
}

void kunmap(struct page *page)
{
	BUG_ON(in_interrupt());
	if (!PageHighMem(page))
		return;
	kunmap_high(page);
}


/*
 * FIX_KMAP infrastructure
 *
 * Provides atomic kmap functions which are faster than regular kmap because no
 * global lock is necessary.
 *
 * kmap_pte is a pointer to the consecutive set of PTEs which cover the
 * FIX_KMAP area. It is set in kmap_init().
 */

static pte_t *kmap_pte;

static void *__kmap_atomic(unsigned long pfn)
{
	unsigned long vaddr;
	int idx, type;

	/* get a new index */
	type = kmap_atomic_idx_push();
	idx = type + KM_TYPE_NR * smp_processor_id();
	vaddr = __fix_to_virt(FIX_KMAP_BEGIN + idx);
#ifdef CONFIG_DEBUG_HIGHMEM
	/* the corresponding pte shouldn't be used already */
	BUG_ON(!pte_none(*(kmap_pte - idx)));
#endif
	set_pte(kmap_pte - idx, pfn_pte(pfn, kmap_prot));

	return (void*)vaddr;
}

void *kmap_atomic(struct page *page)
{
	pagefault_disable();
	if (!PageHighMem(page))
		return page_address(page);

	return __kmap_atomic(page_to_pfn(page));
}

void __kunmap_atomic(void *kvaddr)
{
	unsigned long vaddr = (unsigned long) kvaddr & PAGE_MASK;

	if (vaddr >= __fix_to_virt(FIX_KMAP_END) &&
			vaddr <= __fix_to_virt(FIX_KMAP_BEGIN)) {
		int idx, type;

		type = kmap_atomic_idx();
		idx = type + KM_TYPE_NR * smp_processor_id();
#ifdef CONFIG_DEBUG_HIGHMEM
		WARN_ON_ONCE(vaddr != __fix_to_virt(FIX_KMAP_BEGIN + idx));
#endif
		/* clearing the pte is not strictly necessary but helps
		 * detecting mapping errors */
		pte_clear(&init_mm, vaddr, kmap_pte - idx);
		kmap_atomic_idx_pop();
	}
#ifdef CONFIG_DEBUG_HIGHMEM
	else {
		/* test boundaries */
		BUG_ON(vaddr < PAGE_OFFSET);
		BUG_ON(vaddr >= (unsigned long)high_memory);
	}
#endif
	pagefault_enable();
}

static inline pte_t *kmap_get_fixmap_pte(unsigned long vaddr)
{
	return  pte_offset_kernel(pmd_offset(pud_offset(pgd_offset_k(vaddr),
					vaddr), vaddr), vaddr);
}

extern pte_t *pkmap_page_table;

void kmap_init(void)
{
	/* get a pointer on the PTEs associated to the FIX_KMAP area.
	 * It enables find the right PTE by adding a simple index. Note that it
	 * requires PTEs to be allocated consecutively (see fixmap_kmap_init()
	 * in mm/init.c) */
	kmap_pte = kmap_get_fixmap_pte(__fix_to_virt(FIX_KMAP_BEGIN));

	/* get a pointer on the PTEs associated with the PKMAP area (handled by
	 * linux core) */
	pkmap_page_table = kmap_get_fixmap_pte(PKMAP_BASE);
}
