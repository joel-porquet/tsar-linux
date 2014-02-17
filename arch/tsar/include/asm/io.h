#ifndef _ASM_TSAR_IO_H
#define _ASM_TSAR_IO_H

#include <asm-generic/io.h>

#include <asm/pgtable.h>

void __iomem *__ioremap(phys_addr_t paddr, unsigned long size, pgprot_t prot);

static inline void __iomem *ioremap(phys_addr_t paddr, unsigned long size)
{
	/* default is non-cacheable */
	return __ioremap(paddr, size, PAGE_KERNEL_NOCACHE);
}

static inline void __iomem *ioremap_cache(phys_addr_t paddr, unsigned long size)
{
	return __ioremap(paddr, size, PAGE_KERNEL);
}

#define ioremap_nocache ioremap

extern void iounmap(void __iomem *vaddr);

/* early ioremap */
void ioremap_fixed_early_init(void);

#define readb_relaxed readb
#define readw_relaxed readw
#define readl_relaxed readl

#endif /* _ASM_TSAR_IO_H */
