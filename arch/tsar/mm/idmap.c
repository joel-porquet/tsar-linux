/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <asm/pgalloc.h>

/* heavily inspired by ARM support */

/*
 * The idea here is to build a page table for booting non-boot cpus. Such a
 * page table, called idmap_pg_dir, is the exact copy of the swapper_pg_dir
 * page table to which we add some identity mapping (for the switch from PA to
 * VA). As soon as the non-boot cpus are booted though, they quickly switch to
 * the regular swapper_pg_dir and discard idmap_pg_dir.
 */

pgd_t *idmap_pg_dir;

static void idmap_set_pmd(pgd_t *pgd, unsigned long addr)
{
	pmd_t *pmd = pmd_offset((pud_t*)pgd, addr);

	addr = (addr & PMD_MASK);
	/* force a section mapping (PTE1) */
	set_pmd(pmd, __pmd(addr >> PMD_SHIFT | pgprot_val(PMD_SECT)));
}

static void identity_mapping_add(pgd_t *pgd, const char *text_start,
		const char *text_end)
{
	phys_addr_t addr, end;

	addr = __pa(text_start);
	end = __pa(text_end);

	pr_info("Setting up static identity mapping for 0x%x - 0x%x\n",
			addr, end);

	pgd += pgd_index(addr);
	do {
		unsigned long next;
		next = pgd_addr_end(addr, end);
		idmap_set_pmd(pgd, addr);
		addr = next;
		pgd++;
	} while (addr != end);
}


/*
 * This function is called by the init thread on the boot cpu, before booting
 * non-boot cpus. It builds the idmap_pg_dir page table that is used by
 * non-boot cpus for switching from PA to VA.
 */

extern char __idmap_text_start[], __idmap_text_end[];

static int __init init_static_idmap(void)
{
	/* copy swapper_pg_dir */
	idmap_pg_dir = pgd_alloc(&init_mm);

	if (!idmap_pg_dir)
		return -ENOMEM;

	/* add some identity mapping to cover the function that switches from
	 * PA to VA (see kernel/head.S) */
	identity_mapping_add(idmap_pg_dir, __idmap_text_start,
			__idmap_text_end);

	/* make sure the page table is flushed in memory */
	wmb();

	return 0;
}

/* make our function be called right before initializing SMP */
early_initcall(init_static_idmap);
