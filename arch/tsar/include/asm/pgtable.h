#ifndef _ASM_TSAR_PGTABLE_H
#define _ASM_TSAR_PGTABLE_H

#include <linux/const.h>
#include <linux/sizes.h>

#include <asm-generic/pgtable-nopmd.h>

#include <asm/fixmap.h>
#include <asm/page.h>

#ifndef __ASSEMBLY__

/* 2-level page tables
 *
 * Page Directory:
 * - size: 8KiB
 * - 2048 entries (PTD1 or PTE1) of 32-bit words
 * - each entry covers 2MiB
 *
 * Page Table:
 * - size: 4KiB
 * - 512 entries (PTE2) of 2 * 32-bit words
 * - each entry (64 bits) covers 4KiB
 */

#define	PGD_ORDER	1	/* PGD is two pages */
#define	PTE_ORDER	0	/* PTE is one page */

#define PGD_T_LOG2	(__builtin_ffs(sizeof(pgd_t)) - 1) /* 2 */
#define PTE_T_LOG2	(__builtin_ffs(sizeof(pte_t)) - 1) /* 3 */

#define PTRS_PER_PGD_LOG2	(PAGE_SHIFT + PGD_ORDER - PGD_T_LOG2) /* 11 */
#define PTRS_PER_PTE_LOG2	(PAGE_SHIFT + PTE_ORDER - PTE_T_LOG2) /* 9 */

#define PTRS_PER_PGD		(_AC(1,UL) << PTRS_PER_PGD_LOG2) /* 2048 */
#define PTRS_PER_PTE		(_AC(1,UL) << PTRS_PER_PTE_LOG2) /* 512 */

#define PGDIR_SHIFT		(PTRS_PER_PTE_LOG2 + PAGE_SHIFT) /* 9 + 12 */
#define PGDIR_SIZE		(_AC(1,UL) << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE - 1))

#define PTPR_SHIFT		(PTRS_PER_PGD_LOG2 + PGD_T_LOG2) /* 11 + 2 */


#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_ADDRESS	0

/*
 * PTE flags
 */

typedef unsigned long pteval_t;

/* PTE1/PTE2 common hardware flags */
#define _PAGE_BIT_PRESENT	31	/* is present when 1 */
#define _PAGE_BIT_TYPE		30	/* PTD entry when 1, PTE otherwise */
#define _PAGE_BIT_LOCAL		29	/* was accessed by a cluster local cpu (set by HW) */
#define _PAGE_BIT_REMOTE	28	/* was accessed by a cluster remote cpu (set by HW) */
#define _PAGE_BIT_CACHEABLE	27	/* cacheable when 1 */
#define _PAGE_BIT_WRITABLE	26	/* writable when 1 */
#define _PAGE_BIT_EXECUTABLE	25	/* executable when 1 */
#define _PAGE_BIT_USER		24	/* userspace addressable when 1 */
#define _PAGE_BIT_GLOBAL	23	/* not invalidated by TLB flush when 1 */
#define _PAGE_BIT_DIRTY		22	/* was written to when 1 */

#define _PAGE_PRESENT		(_AT(pteval_t, 1) << _PAGE_BIT_PRESENT)
#define _PAGE_PTD		(_AT(pteval_t, 1) << _PAGE_BIT_TYPE)
#define _PAGE_PTE		(_AT(pteval_t, 0) << _PAGE_BIT_TYPE)
#define _PAGE_LOCAL		(_AT(pteval_t, 1) << _PAGE_BIT_LOCAL)
#define _PAGE_REMOTE		(_AT(pteval_t, 1) << _PAGE_BIT_REMOTE)
#define _PAGE_CACHEABLE		(_AT(pteval_t, 1) << _PAGE_BIT_CACHEABLE)
#define _PAGE_WRITABLE		(_AT(pteval_t, 1) << _PAGE_BIT_WRITABLE)
#define _PAGE_EXECUTABLE	(_AT(pteval_t, 1) << _PAGE_BIT_EXECUTABLE)
#define _PAGE_USER		(_AT(pteval_t, 1) << _PAGE_BIT_USER)
#define _PAGE_GLOBAL		(_AT(pteval_t, 1) << _PAGE_BIT_GLOBAL)
#define _PAGE_DIRTY		(_AT(pteval_t, 1) << _PAGE_BIT_DIRTY)

#define _PAGE_PTD_MASK		(~(_PAGE_PRESENT | _PAGE_PTD))

/* software flags */
#define _PAGE_BIT_ACCESSED	21
#define _PAGE_ACCESSED		(_AT(pteval_t, 1) << _PAGE_BIT_ACCESSED)
#define _PAGE_BIT_FILE		20
#define _PAGE_FILE		(_AT(pteval_t, 1) << _PAGE_BIT_FILE)

#define _PAGE_CHG_MASK		(_PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_CACHEABLE)

/*
 * Page permissions
 */

/* second level */
#define __PAGE_BASE \
	(_PAGE_PRESENT | _PAGE_ACCESSED)

#define __PAGE_KERNEL_BASE \
	(__PAGE_BASE | _PAGE_GLOBAL | _PAGE_DIRTY | _PAGE_WRITABLE)

#define __PAGE_KERNEL \
	(__PAGE_KERNEL_BASE | _PAGE_CACHEABLE | _PAGE_EXECUTABLE)

#define __PAGE_USER \
	(__PAGE_BASE | _PAGE_USER | _PAGE_CACHEABLE)

#define PAGE_NONE \
	__pgprot(__PAGE_BASE)
#define PAGE_READONLY \
	__pgprot(__PAGE_USER)
#define PAGE_READONLY_EXEC \
	__pgprot(__PAGE_USER | _PAGE_EXECUTABLE)
#define PAGE_SHARED \
	__pgprot(__PAGE_USER | _PAGE_WRITABLE)
#define PAGE_SHARED_EXEC \
	__pgprot(__PAGE_USER | _PAGE_WRITABLE | _PAGE_EXECUTABLE)
#define PAGE_COPY \
	__pgprot(__PAGE_USER)
#define PAGE_COPY_EXEC \
	__pgprot(__PAGE_USER | _PAGE_EXECUTABLE)

#define PAGE_KERNEL \
	__pgprot(__PAGE_KERNEL)
#define PAGE_KERNEL_NOCACHE \
	__pgprot(__PAGE_KERNEL_BASE)

/* first level */
#define __PMD_BASE \
	(_PAGE_PRESENT)

#define __PMD_SECT \
	(__PMD_BASE | _PAGE_PTE | __PAGE_KERNEL)

#define __PMD_TABLE \
	(__PMD_BASE | _PAGE_PTD)

#define PMD_SECT \
	__pgprot(__PMD_SECT)

#define PMD_TABLE \
	__pgprot(__PMD_TABLE)


/* write imply read,
 * copy is the same as ro (so we can detect copy on write) */
/*         xwr */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY_EXEC
#define __P101	PAGE_READONLY_EXEC
#define __P110	PAGE_COPY_EXEC
#define __P111	PAGE_COPY_EXEC

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY_EXEC
#define __S101	PAGE_READONLY_EXEC
#define __S110	PAGE_SHARED_EXEC
#define __S111	PAGE_SHARED_EXEC

/*
 * PKmap range (only for highmem)
 *
 * One page worth of PTE2s, aligned on a PMD
 */
#define LAST_PKMAP	PTRS_PER_PTE
#define PKMAP_BASE	((FIXADDR_START - PAGE_SIZE * (LAST_PKMAP + 1)) \
		& PMD_MASK)

/*
 * Vmalloc range
 */
#define VMALLOC_OFFSET	SZ_8M
#define VMALLOC_START	((unsigned long)high_memory + VMALLOC_OFFSET)
#ifdef CONFIG_HIGHMEM
#define VMALLOC_END	(PKMAP_BASE - 2 * PAGE_SIZE)
#else
#define VMALLOC_END	(FIXADDR_START - 2 * PAGE_SIZE)
#endif

/*
 * Zero page
 */
extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

/*
 * Kernel pg table
 */
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

/*
 * page table stuff
 */
#define pte_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pte %08llx.\n", \
			__FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", \
			__FILE__, __LINE__, pgd_val(e))

/* PMD management */
#define pmd_none(pmd)		(!pmd_val(pmd))
#define pmd_present(pmd)	(pmd_val(pmd) & _PAGE_PRESENT)
/* XXX: not 100% sure about that... */
#define pmd_bad(pmd)		(((pmd_val(pmd) & (_PAGE_PRESENT | _PAGE_PTD))	\
				!= (_PAGE_PRESENT | _PAGE_PTD)))
/* no need to flush, we're cc */
#define pmd_clear(pmdp)		do { pmd_val(*(pmdp)) = 0; } while (0)
#define set_pmd(pmdp, pmdval)	do { *(pmdp) = pmdval; } while (0)

/* PTE management */
#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pte_pfn(x)		((unsigned long)(((x).pte_high)))
#define pfn_pte(pfn, prot)	__pte(((unsigned long long)(pfn) << 32) \
				| pgprot_val(prot))

#define pte_none(pte)		(!pte_val(pte))
#define pte_present(pte)	((pte).pte_low & (_PAGE_PRESENT))

#define pte_clear(mm, addr, ptep) \
	do { set_pte_at(mm, addr, ptep, __pte(0)); } while (0)

#define set_pte_at(mm, addr, ptep, pteval) set_pte(ptep, pteval)
#define set_pte(ptep, pteval)				\
	do {						\
		(ptep)->pte_high = (pteval).pte_high;	\
		smp_wmb();				\
		(ptep)->pte_low = (pteval).pte_low;	\
	} while (0)

#define mk_pte(page, prot)	pfn_pte(page_to_pfn(page), prot)

/*
 * Finding entries in page tables
 */
#define pmd_page(pmd)		(pfn_to_page(pmd_val(pmd) & _PAGE_PTD_MASK))
#define pmd_page_kernel(pmd)	((unsigned long)pfn_to_virt(pmd_val(pmd) & _PAGE_PTD_MASK))

/* page global directory */
#define pgd_index(addr)		(((addr) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))

#define pgd_offset_kernel(addr)	pgd_offset(&init_mm, addr)
#define pgd_offset_k		pgd_offset_kernel

/* page table */
#define pte_index(addr)		(((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

#define pte_offset_kernel(dir, addr)	\
	((pte_t *)pmd_page_kernel(*(dir)) + pte_index(addr))

#ifdef CONFIG_HIGHPTE
/* if the pte is part of high memory, it must be kmapped first */
#define pte_offset_map(dir, addr)	\
	((pte_t *)kmap_atomic(pmd_page(*(dir))) + pte_index(addr))
#define pte_unmap(pte) kunmap_atomic(pte)
#else
#define pte_offset_map(dir, addr)	\
	((pte_t *)page_address(pmd_page(*(dir))) + pte_index(addr))
#define pte_unmap(pte) ((void)(pte))
#endif

/*
 * pte attributes (only works if pte_present() is true)
 */
#define pte_write(pte)		((pte).pte_low & _PAGE_WRITABLE)
#define pte_dirty(pte)		((pte).pte_low & _PAGE_DIRTY)
#define pte_young(pte)		((pte).pte_low & _PAGE_ACCESSED)
#define pte_file(pte)		((pte).pte_low & _PAGE_FILE)
#define pte_special(pte)	(0) /* XXX: not sure what this is... */

#define PTE_BIT_FUNC(fn, op) \
static inline pte_t pte_##fn(pte_t pte) { pte.pte_low op; return pte; }

PTE_BIT_FUNC(wrprotect, &= ~_PAGE_WRITABLE);
PTE_BIT_FUNC(mkwrite,   |= _PAGE_WRITABLE);
PTE_BIT_FUNC(mkclean,   &= ~_PAGE_DIRTY);
PTE_BIT_FUNC(mkdirty,   |= _PAGE_DIRTY);
PTE_BIT_FUNC(mkold,     &= ~_PAGE_ACCESSED);
PTE_BIT_FUNC(mkyoung,   |= _PAGE_ACCESSED);

static inline pte_t pte_mkspecial(pte_t pte) { return pte; }

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte.pte_low &= _PAGE_CHG_MASK;
	pte.pte_low |= pgprot_val(newprot);
	return pte;
}

/*
 * make a page protection as uncacheable
 */
#define pgprot_noncached(prot) \
	__pgprot(pgprot_val(prot) & ~_PAGE_CACHEABLE)

#define pgprot_writecombine pgprot_noncached

/*
 * swap entries
 */
#define __swp_type(x)			((x).val & 0x1f)
#define __swp_offset(x)			((x).val >> 5)
#define __swp_entry(type, offset)	((swp_entry_t){ (type) | (offset) << 5})
#define __pte_to_swp_entry(pte)		((swp_entry_t){ (pte).pte_high})
#define __swp_entry_to_pte(x)		((pte_t){ 0, (x).val})

#define pte_to_pgoff(pte)		((pte).pte_high)
#define pgoff_to_pte(off)		((pte_t){ _PAGE_FILE, (off)})

#define PTE_FILE_MAX_BITS		32

/*
 * various
 */
#define kern_addr_valid(addr)	(1) /* XXX: might need better support if not FLATMEM */

#define pgtable_cache_init()	do {} while (0)

extern void paging_init(void);

static inline void update_mmu_cache(struct vm_area_struct *vma,
		unsigned long addr, pte_t *ptep)
{
}


#include <asm-generic/pgtable.h>

#endif /* !__ASSEMBLY__ */

#endif /* _ASM_TSAR_PGTABLE_H */
