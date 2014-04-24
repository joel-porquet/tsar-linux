#ifndef _ASM_TSAR_PAGE_H
#define _ASM_TSAR_PAGE_H

#include <linux/const.h>

/* definition of a page (4KiB) */
#define PAGE_SHIFT	12
#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE - 1))

#ifndef __ASSEMBLY__

#define get_user_page(vaddr)		__get_free_page(GFP_KERNEL)
#define free_user_page(page, addr)	free_page(addr)

#define clear_page(page)	memset((page), 0, PAGE_SIZE)
#define copy_page(to,from)	memcpy((to), (from), PAGE_SIZE)

#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)

/*
 * These are used to make use of C type-checking..
 */
typedef struct {
	unsigned long pte_low;	/* protection bits */
	unsigned long pte_high; /* PPN */
} pte_t;
typedef struct {
	unsigned long pgd;
} pgd_t;
typedef struct {
	unsigned long pgprot;
} pgprot_t;
typedef struct page *pgtable_t;

#define pte_val(x) \
	((x).pte_low | ((unsigned long long)(x).pte_high << 32))
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x) \
	({ pte_t __pte = {(x), ((unsigned long long)(x)) >> 32}; __pte; })
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )

#endif /* !__ASSEMBLY__ */

/* PAGE_OFFSET is the virtual address of the start of kernel address space */
#define PAGE_OFFSET _AC(CONFIG_PAGE_OFFSET, UL)

#ifndef __ASSEMBLY__

#define __va(x) ((void *)((unsigned long) (x) + PAGE_OFFSET))
#define __pa(x) ((unsigned long) (x) - PAGE_OFFSET)

#define virt_to_pfn(kaddr)	(__pa(kaddr) >> PAGE_SHIFT)
#define pfn_to_virt(pfn)	__va((pfn) << PAGE_SHIFT)

#define virt_to_page(addr)	pfn_to_page(virt_to_pfn(addr))
#define page_to_virt(page)	pfn_to_virt(page_to_pfn(page))

#ifndef page_to_phys
#define page_to_phys(page)      ((dma_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#endif

extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;

#define pfn_valid(pfn)		(((pfn) >= min_low_pfn) && ((pfn) < max_low_pfn))

#define	virt_addr_valid(kaddr)	(pfn_valid(virt_to_pfn(kaddr)))

#endif /* __ASSEMBLY__ */

#define VM_DATA_DEFAULT_FLAGS	(VM_READ | VM_WRITE | VM_EXEC | \
		VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* _ASM_TSAR_PAGE_H */
