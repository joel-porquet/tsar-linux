/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/asmmacro.h>
#include <asm/mips32c0_regs.h>
#include <asm/mmuc2_regs.h>
#include <asm/regdef.h>
#include <asm/thread_info.h>

#include <generated/asm-offsets.h>

/* physical address */
#define pa(X) ((X) - PAGE_OFFSET)

__HEAD
ENTRY(kernel_entry)
	/*
	 * All the cpus enter here. Non-boot cpus are quickly redirected to
	 * the secondary entry point.
	 */

	/*
	 * zero CP0_STATUS: kernel mode, interrupts disabled
	 */
	mtc0	zero, CP0_STATUS

#ifdef CONFIG_TSAR_SOCLIB
	/* if we're on the simulation platform, deactivate iss debugging */
	mtc0	zero, CP0_DEBUG
#endif

#ifdef CONFIG_SMP
	/* get the hardware cpuid to determine whether we are the boot cpu */
	mfc0	t0, CP0_EBASE, 1
	andi	t0, EBASE_CPUHWID	/* extract cpu hardware id */
	bnez	t0, secondary_kernel_entry
#endif

	/*
	 * clear .bss segment
	 */
	la	t0, pa(__bss_start)
	la	t1, pa(__bss_stop)
	beq	t0, t1, 2f
1:
	sw	zero, (t0)
	addiu	t0, 4
	bne	t0, t1, 1b
2:

	/*
	 * save bootup parameters before switching to VA
	 */
#if 0
	/* TODO: we should be able to receive the device tree from the
	 * bootloader here */
	sw	a0, pa(boot_param0)
	sw	a1, pa(boot_param1)
	sw	a2, pa(boot_param2)
	sw	a3, pa(boot_param3)
#endif

	/*
	 * initialize page global directory (swapper_pg_dir)
	 *
	 * map RAM from the start to the end of the kernel (_end):
	 * - identity mapping at VA 0x0 for just after enabling the MMU (the
	 *   mapping will be destroy later)
	 * - regular mapping at VA PAGE_OFFSET
	 *
	 * mapping is done with big pages
	 */

sizeof_pgd_t = (1 << PGD_T_LOG2)
page_pde_offset = (PAGE_OFFSET >> (PGDIR_SHIFT - PGD_T_LOG2))

	la	t0, pa(swapper_pg_dir)
	la	t1, pa(_end) + (PGDIR_SIZE - 1)	/* big page that follows _end */
	srl	t1, PGDIR_SHIFT			/* finds the index of this big page */
	sll	t1, PGD_T_LOG2
	addu	t1, t1, t0
	li	t2, __PAGE_KERNEL		/* default page properties */
10:
	sw	t2, (t0)			/* identity mapping */
	sw	t2, page_pde_offset(t0)		/* regular mapping */
	addu	t2, 1				/* next address mapping */
	addiu	t0, sizeof_pgd_t		/* next PTE1 */
	bne	t0, t1, 10b			/* until we reach the last big page */

	/*
	 * enable paging
	 */

	la	t0, pa(swapper_pg_dir)		/* physical address of the reference page table */
	la	t1, va_jump			/* note we use VA address of symbol va_jump */
	b	enable_paging
va_jump:

	/*
	 * setup global pointer and kernel stack
	 */
	la	gp, init_thread_union

	SETUP_KSTACK

	/* start linux! */
	j	start_kernel

ENDPROC(kernel_entry)

/*
 * Put the code where we jump from the initial 1:1 physical address space to
 * the virtual address space in a special section (in prevision for SMP
 * support).
 *
 * - t0 contains the physical address of the page table to use
 * - t1 contains the _virtual_ address to return to upon completion
 */
	.pushsection	.idmap.text, "ax"
ENTRY(enable_paging)

mmu_mode_init = (MMU_MODE_INST_TLB | MMU_MODE_DATA_TLB | MMU_MODE_INST_CACHE | MMU_MODE_DATA_CACHE)
ptpr_shift = (PTRS_PER_PGD_LOG2 + PGD_T_LOG2)

	mtc2	$0, MMU_SYNC		/* sync all pending write operations */

	srl	t0, t0, ptpr_shift	/* pg_dir >> 13 */
	mtc2	t0, MMU_PTPR		/* set PTPR (8KiB aligned) */

	li	t0, mmu_mode_init	/* init mmu mode */
	mtc2	t0, MMU_MODE		/* enable MMU */

	/* jump into VA space */
	jr	t1

ENDPROC(enable_paging)
	.popsection

#ifdef CONFIG_SMP
ENTRY(secondary_kernel_entry)
	/*
	 * Entry point for non-boot cpus
	 * - t0 contains the hardware cpuid
	 */

	/*
	 * spin until our cpuid is called
	 */
	la	t1, pa(secondary_cpu_boot)
spin_wait_boot:
	lw	t2, 0(t1)
	bne	t2, t0, spin_wait_boot

	/*
	 * enable paging
	 */

	lw	t0, pa(idmap_pg_dir)	/* physical address of the idmap page table */
	la	t1, secondary_va_jump	/* note we use VA symbol */
	b	enable_paging
secondary_va_jump:

	/*
	 * setup kernel stack and global pointer
	 */

	la	t0, secondary_data
	lw	sp, 0(t0)		/* get sp */
	lw	gp, 4(t0)		/* get gp */

	/*
	 * jump into Linux (and into C code)
	 */
	j	secondary_start_kernel

ENDPROC(secondary_kernel_entry)
#endif
