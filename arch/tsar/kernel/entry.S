/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/linkage.h>

#include <generated/asm-offsets.h>

#include <asm/asmmacro.h>
#include <asm/mips32c0_regs.h>
#include <asm/regdef.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>

/*
 * interrupt and kernel mode management
 */


/*
 * general exception vector entry
 * following MIPS specifications, its address must be at offset 0x180 of a
 * memory area aligned on a page.
 */
	.balign PAGE_SIZE
	.space	0x180
ENTRY(general_exception_vector)
	.set push
	.set noat
	mfc0	k1, CP0_CAUSE
	andi	k1, k1, 0x7c
	lw	k0, exception_handlers(k1)
	jr	k0
	.set pop
ENDPROC(general_exception_vector)

/*
 * exception handlers:
 * -:   RES	Reserved
 *
 * 0:	Int	Interrupt
 * 4: 	AdEL 	Address error exception (load or instruction fetch)
 * 5: 	AdES 	Address error exception (store)
 * 6: 	IBE 	Bus error exception (instruction fetch)
 * 7: 	DBE 	Bus error exception (data reference: load or store)
 * 8: 	Sys 	Syscall exception
 * 9: 	Bp 	Breakpoint exception.
 * 10: 	RI 	Reserved instruction exception
 * 11: 	CpU 	Coprocessor Unusable exception
 * 12: 	Ov 	Arithmetic Overflow exception
 * 13: 	Tr 	Trap exception
 * 15: 	FPE 	Floating point exception
 */

ENTRY(handle_reserved)
	SAVE_ALL
	STI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_reserved
ENDPROC(handle_reserved)

ENTRY(handle_int)
	SAVE_ALL
	CLI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_intr
	la	t0, handle_irq_icu // get the IRQ handler
	lw	t0, (t0)
	jr	t0
ENDPROC(handle_int)

ENTRY(handle_ade)
	SAVE_ALL
	mfc0	t0, CP0_BADVADDR // put badvaddr in pt_regs
	sw	t0, PT_BVADDR(sp)
	KMODE
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_ade
ENDPROC(handle_ade)

ENTRY(handle_ibe)
	SAVE_ALL
	CLI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_ibe
ENDPROC(handle_ibe)

ENTRY(handle_dbe)
	SAVE_ALL
	CLI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_dbe
ENDPROC(handle_dbe)

ENTRY(handle_bp)
	SAVE_ALL
	STI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_bp
ENDPROC(handle_bp)

ENTRY(handle_ri)
	SAVE_ALL
	STI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_ri
ENDPROC(handle_ri)

ENTRY(handle_cpu)
	SAVE_ALL
	STI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_cpu
ENDPROC(handle_cpu)

ENTRY(handle_ov)
	SAVE_ALL
	STI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_ov
ENDPROC(handle_ov)

ENTRY(handle_tr)
	SAVE_ALL
	STI
	move	a0, sp // give the current pt_regs as arg
	la	ra, ret_from_exception
	j	do_tr
ENDPROC(handle_tr)


/*
 * Syscall handling
 */

ENTRY(handle_sys)
	SAVE_ALL
	STI

	/* skip syscall on return
	 * (we assume the syscall can't be in a delay slot) */
	lw	t0, PT_EPC(sp)
	addiu	t0, 4
	sw	t0, PT_EPC(sp)

	/* keep track of the syscall number for tracing and in case we have to
	 * restart it. use PT_R0 for that, and add 1 to it so we can
	 * differentiate whether we're in a syscall or not (its value is set to
	 * 0 by SAVE_ALL above by default, and the syscall numbers also starts
	 * at 0) */
	lw	t0, PT_R2(sp)
	addiu	t0, 1
	sw	t0, PT_R0(sp)

	/* check syscall number */
	lw	v0, PT_R2(sp)
	sltiu	t0, v0, __NR_syscalls
	beqz	t0, illegal_syscall

	/* get syscall routine @ and number of args
	 * (each entry is 8 bytes wide) */
	sll	t0, v0, 3
	la	t1, sys_call_table
	add	t1, t1, t0
	lw	t2, (t1)
	lw	t3, 4(t1)

	/* t3 >= 0 means we need to fetch remaining arguments from user stack */
	bgez	t3, args_ustack
stack_done:

	// TODO: syscall_trace

	/* do the syscall */
	jalr	t2

	/* store back the result in the saved context */
	sw	v0, PT_R2(sp)

	/* return */
	j	syscall_exit

args_ustack:
	/* get ustack pointer */
	lw	t0, PT_R29(sp)

	/* check the user stack doesn't go into kernel space */
	lw	t5, TI_ADDR_LIMIT($28)
	addu	t4, t0, 8*4 // 8 arguments max
	bgeu	t4, t5, bad_ustack

	/* copy the args from ustack onto kstack:
	 * let's not optimize as MIPS does and just copy the remaining 4
	 * arguments */
	lw	t5, 16(t0)
	sw	t5, 16(sp)
	lw	t5, 20(t0)
	sw	t5, 20(sp)
	lw	t5, 24(t0)
	sw	t5, 24(sp)
	lw	t5, 28(t0)
	sw	t5, 28(sp)

	/* go back to regular path */
	j	stack_done

bad_ustack:
	/* return error code */
	li	v0, -EFAULT
	sw	v0, PT_R2(sp)
	j	syscall_exit

illegal_syscall:
	/* return error code */
	li	v0, -ENOSYS
	sw	v0, PT_R2(sp)
	j	syscall_exit
ENDPROC(handle_sys)

ENTRY(sys_rt_sigreturn)
	/* sys_rt_sigreturn can't be generic because we have to
	 * modify a stack frame */
	move	a0, sp
	j	_sys_rt_sigreturn
ENDPROC(sys_rt_sigreturn)


/*
 * everything that returns from kernel
 */

ENTRY(ret_from_exception)
	LOCAL_IRQ_DISABLE
ENTRY(ret_from_intr)
	lw	t0, PT_STATUS(sp) // returning to kernel mode?
	andi	t0, ST0_KSU_USER
	beqz	t0, restore_all

resume_userspace:
	LOCAL_IRQ_DISABLE
	lw	a1, TI_FLAGS($28)
	li	t0, _TIF_WORK_MASK
	and	t0, a1, t0
	bnez	t0, work_pending
	j	restore_all

restore_all:
	RESTORE_ALL_AND_RET

work_pending:
	/* a1 is already preloaded with TI_FLAGS(thread_info) */
	andi	t0, a1, _TIF_NEED_RESCHED
	beqz	t0, work_notifysig /* if it's not resched, then it's notify or sig */
	jal	schedule
	j	resume_userspace

work_notifysig:
	/* pending signals and notify-resume requests */
	move	a0, sp
	/* a1 is preloaded with TI_FLAGS(thread_info) */
	jal	do_notify_resume
	j	resume_userspace

ENTRY(ret_from_fork)
	/* first function to call for newly created threads */
	jal	schedule_tail	// a0 = struct task_struct *prev

	/* check if we're a kthread (if so $16 != 0) */
	beq	s0, zero, syscall_exit

	/* call the requested function with its corresponding argument */
	move	a0, s1
	jal	s0

syscall_exit:
	LOCAL_IRQ_DISABLE
	lw	a1, TI_FLAGS($28)
	li	t0, _TIF_WORK_MASK
	and	t0, a1, t0
	bnez	t0, syscall_exit_work
	j	restore_all

syscall_exit_work:
	// TODO: deal with syscall tracing
	j	work_pending

