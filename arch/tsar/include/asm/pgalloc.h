#ifndef _ASM_TSAR_PGALLOC_H
#define _ASM_TSAR_PGALLOC_H

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/tlb.h>

#define PGD_ORDER	1 /* 2 pages */

/*
 * alloc and free pgd
 */
static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *ret;

	/* get two pages */
	/* I've been said on #mipslinux that the result address should be
	 * aligned on the number of allocated pages (ie 8KiB here) */
	ret = (pgd_t *) __get_free_pages(GFP_KERNEL, PGD_ORDER);

	/* check we got enough memory and the area is 8KiB aligned */
	if (!ret || !IS_ALIGNED((unsigned long)ret, 2 * PAGE_SIZE))
		return NULL;

	/* zero out the user space */
	memset(ret, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));

	/*
	 * copy the kernel
	 */
	memcpy(ret + USER_PTRS_PER_PGD, swapper_pg_dir + USER_PTRS_PER_PGD,
			(PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	return ret;
}

static inline void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	free_pages((unsigned long)pgd, PGD_ORDER);
}

/*
 * allocate one PTE table
 */

#define PGALLOC_GFP GFP_KERNEL | __GFP_NOTRACK | __GFP_REPEAT | __GFP_ZERO

static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte;

	pte = (pte_t*) __get_free_page(PGALLOC_GFP);

	return pte;
}

static inline pgtable_t pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	struct page *pte;
	pte = alloc_pages(PGALLOC_GFP, 0);
	if (pte) {
		pgtable_page_ctor(pte);
	}
	return pte;
}

/*
 * free one PTE table
 */
static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	BUG_ON((unsigned long)pte & (PAGE_SIZE - 1));
	free_page((unsigned long)pte);
}

static inline void pte_free(struct mm_struct *mm, struct page *pte)
{
	pgtable_page_dtor(pte);
	__free_page(pte);
}

/* it's a define because it ends up using __tlb_remove_page which is not
 * static, hence it gives us a compilation error */
#define __pte_free_tlb(tlb, pte, address) \
do {                                      \
	pgtable_page_dtor(pte);           \
	tlb_remove_page(tlb, pte);        \
} while(0)

/*
 * populate the pmdp entry with a pointer to the pte
 */
static inline void __pmd_populate(pmd_t *pmdp, unsigned long pmdval)
{
	set_pmd(pmdp, __pmd(pmdval));
}

#define _PAGE_KERNEL_TABLE	(__PMD_TABLE)
#define _PAGE_USER_TABLE	(__PMD_TABLE)

static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmdp, pte_t *ptep)
{
	unsigned long pte_ptr = (unsigned long)ptep;

	__pmd_populate(pmdp, __pa(pte_ptr) >> PAGE_SHIFT | _PAGE_KERNEL_TABLE);
}

static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmdp, struct page *ptep)
{
	__pmd_populate(pmdp, page_to_pfn(ptep) | _PAGE_USER_TABLE);
}

#define pmd_pgtable(pmd) pmd_page(pmd)

#define check_pgt_cache()	do { } while (0)

#endif /* _ASM_TSAR_PGALLOC_H */
