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

/*
 * Fixmap management
 */

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


/*
 * ioremap API
 */

void __iomem * __init_refok
__ioremap(phys_addr_t paddr, unsigned long size, pgprot_t prot)
{
	struct vm_struct *area;
	phys_addr_t last_paddr;
	unsigned long offset, vaddr;

	/* Don't allow wraparound or zero size */
	last_paddr = paddr + size - 1;
	if (!size || last_paddr < paddr)
		return NULL;

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

void __iounmap(void __iomem *addr)
{
	void *vaddr = (void*)((unsigned long)addr & PAGE_MASK);
	vunmap(vaddr);
}
