/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/linkage.h>

#include <generated/asm-offsets.h>

#include <asm/regdef.h>

/*
 * __asm_switch_to switches between two tasks. The context of 'prev' is saved
 * on its kernel stack. Then the context of 'next' is restored from its kernel
 * stack.  Finally we return to the second task via return.
 *
 * There are two ways to ending up here: either by coming from the actual
 * scheduling switch (where the 'prev' task was actually running) or via
 * forking, in which case an environment was setup to conform to this switch
 * path (copy_thread())
 */

/* ABI rules apply to this function, ie callee-saved registers must be saved
 * and restored. */

/* struct thread_info *__asm_switch_to(struct thread_info prev_ti,
		struct thread_info next_ti); */

ENTRY(__asm_switch_to)
	/* Important note: we don't save nor do we restore PT_STATUS. According
	 * to Openrisc, its value is supposed to be the same going in and going
	 * out from here. */

	/* make room for a full pt_regs struct */
	addiu	sp, sp, -(PT_SIZE)

	/* save callee-saved registers */
	sw	s0, PT_R16(sp)
	sw	s1, PT_R17(sp)
	sw	s2, PT_R18(sp)
	sw	s3, PT_R19(sp)
	sw	s4, PT_R20(sp)
	sw	s5, PT_R21(sp)
	sw	s6, PT_R22(sp)
	sw	s7, PT_R23(sp)

	/* save the frame pointer */
	sw	fp, PT_R30(sp)

	/* save the return address */
	sw	ra, PT_R31(sp)

	/* we use thread_info->ksp to save the address of the above pt_regs
	 * structure so that we can get back to it later. But we don't want to
	 * lose the current value of thread_info->ksp, so we stack it as
	 * pt_regs->sp */

	/* save the old value of thread_info->ksp as pt_regs->sp */
	lw	t0, TI_KSP(a0)
	sw	t0, PT_R29(sp)

	/* swap kernel stack pointers */
	sw	sp, TI_KSP(a0)
	lw	sp, TI_KSP(a1)

	/* restore the old value of thread_info->ksp */
	lw	t0, PT_R29(sp)
	sw	t0, TI_KSP(a1)

	/* restore callee-savec registers */
	lw	s0, PT_R16(sp)
	lw	s1, PT_R17(sp)
	lw	s2, PT_R18(sp)
	lw	s3, PT_R19(sp)
	lw	s4, PT_R20(sp)
	lw	s5, PT_R21(sp)
	lw	s6, PT_R22(sp)
	lw	s7, PT_R23(sp)

	/* restore the frame pointer */
	lw	fp, PT_R30(sp)

	/* restore the return address */
	lw	ra, PT_R31(sp)

	/* unwind the stack */
	addiu	sp, sp, (PT_SIZE)

	/* save 'prev_ti' to 'last_ti' return value */
	move	v0, a0

	/* make a0 be (struct task_struct *prev) for schedule_tail (in case we
	 * return to ret_from_fork() because we're in a newly created thread)
	 */
	lw	a0, TI_TASK(a0)

	jr	ra

ENDPROC(__asm_switch_to)


