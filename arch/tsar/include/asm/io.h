#ifndef _ASM_TSAR_IO_H
#define _ASM_TSAR_IO_H

#include <linux/compiler.h>
#include <linux/types.h>

#include <asm/page.h>
#include <asm/pgtable.h>

extern void __iomem *__ioremap(phys_addr_t paddr, unsigned long size, pgprot_t prot);
extern void __iounmap(void __iomem *vaddr);

/* by default ioremap() is uncached */
#define ioremap(paddr, size) \
	__ioremap((paddr), (size), PAGE_KERNEL_NOCACHE)

#define ioremap_nocache(paddr, size) \
	__ioremap((paddr), (size), PAGE_KERNEL_NOCACHE)

#define ioremap_cache(paddr, size) \
	__ioremap((paddr), (size), PAGE_KERNEL)

#define iounmap(vaddr) \
	__iounmap(vaddr)

/* probably not necessary, only used by a few drivers */
#define readb_relaxed readb
#define readw_relaxed readw
#define readl_relaxed readl

/* use phys_addr_t instead of only unsigned long as in the asm-generic
 * definition */
#define virt_to_phys __pa
#define phys_to_virt __va

#include <asm-generic/io.h>

#endif /* _ASM_TSAR_IO_H */
