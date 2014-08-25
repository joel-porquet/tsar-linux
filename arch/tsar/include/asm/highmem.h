#ifndef _ASM_TSAR_HIGHMEM_H

#include <asm/fixmap.h>

/*
 * Virtual space layout (from top address):
 *
 * (empty page)
 * FIXADDR_TOP
 * 		fixed addresses
 * 		(including FIX_KMAP)
 * FIXADDR_START
 * 		Persistent kmap area
 * PKMAP_BASE
 * VMALLOC_END
 * 		Vmalloc area
 * VMALLOC_START
 * high_memory
 *		low_memory
 * PAGE_OFFSET
 * ... (user space) ...
 */

#define LAST_PKMAP_MASK	(LAST_PKMAP - 1)
#define PKMAP_NR(virt)	((virt - PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)	(PKMAP_BASE + (nr << PAGE_SHIFT))

extern void *kmap(struct page *page);
extern void kunmap(struct page *page);

extern void *kmap_atomic(struct page *page);
extern void __kunmap_atomic(void *kvaddr);

#define flush_cache_kmaps()	do { } while (0)

extern void kmap_init(void);

#define kmap_prot PAGE_KERNEL

#define _ASM_TSAR_HIGHMEM_H
#endif /* _ASM_TSAR_HIGHMEM_H */
