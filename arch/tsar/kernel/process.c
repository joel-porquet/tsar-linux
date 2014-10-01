/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/irqflags.h>
#include <linux/ptrace.h>
#include <linux/sched.h>

#include <asm/elf.h>
#include <asm/ptrace.h>
#include <asm/switch_to.h>
#include <asm/thread_info.h>
#include <asm/uaccess.h>

/*
 * Power management
 */

void arch_cpu_idle(void)
{
	/* enable IRQs before going to sleep, so that the cpu gets woken up
	 * when required */
	local_irq_enable();
	/* put the cpu to sleep (with 3 nops due to pipeline) */
	__asm__ __volatile__ (
			".set push	\n"
			"	wait	\n"
			"	nop	\n"
			"	nop	\n"
			"	nop	\n"
			".set pop	\n"
			);
}

/*
 * Thread management
 */

/*
 * Start a new user thread. This function is called after loading a user
 * executable (elf, aout, etc.) to setup a code entry (pc) and a new stack
 * (sp).
 */
void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long sp)
{
	unsigned long status;

	/* user mode but with EXL set (so we ensure interrupts are still
	 * disable just before eret from kernel) */
	status = regs->cp0_status | read_c0_status();
	status &= ~(ST0_KSU);
	status |= ST0_KSU_USER;
	status |= ST0_EXL;

	regs->cp0_status = status;

	/* setup pc and sp */
	regs->cp0_epc = pc;
	regs->regs[29] = sp;
}

/*
 * Create a new thread by copying from an existing one (typically the init_thread).
 * - clone_flags give some info about the new thread (ie user or kernel thread)
 * - usp is the user stack pointer (if user thread) or a function pointer (if kthread)
 * - arg is the argument of the function, in case of kthread; always NULL for user thread
 * - p is the new task_struct
 *
 * We use the same approach as Openrisc, that is two pt_regs are stacked at the
 * top of the kstack. The first (topmost) is the userspace context, and the
 * second is the kernelspace context.
 *
 * A kthread is not supposed to return to userspace, so the user context
 * (topmost) stays uninitialized, but we still need it in case the kthread
 * finally becomes a user thread.
 *
 * The second context is used to return to ret_from_fork. A kthread set $16 to
 * the @ of a function to jump to (with arg in $17); uthread sets $16 to NULL,
 * in which case ret_from_fork will just continue returning to userspace.
 */
extern asmlinkage void ret_from_fork(void);

int copy_thread(unsigned long clone_flags, unsigned long usp,
		unsigned long arg, struct task_struct *p)
{
	struct pt_regs *uregs; /* first topmost pt_regs context */
	struct pt_regs *kregs; /* second pt_regs context */
	unsigned long ksp = (unsigned long)task_stack_page(p) + THREAD_SIZE; /* top of the kstack */
	unsigned long top_of_kstack = ksp;

	/* make room for the first (user) pt_regs */
	ksp -= sizeof(struct pt_regs);
	uregs = (struct pt_regs*)ksp;

	/* make room for the second (kernel) pt_regs */
	ksp -= sizeof(struct pt_regs);
	kregs = (struct pt_regs*)ksp;

	if (unlikely(p->flags & PF_KTHREAD)) {
		/* Kernel thread */

		/* let's clear the kernel context (second pt_regs on stack) */
		memset(kregs, 0, sizeof(struct pt_regs));
		/* and initialize the function pointer and its argument */
		kregs->regs[16] = usp;
		kregs->regs[17] = arg;

		/* let's clear the user context (in case this kthread becomes a
		 * user thread later, e.g. kernel_init becomes init) */
		memset(uregs, 0, sizeof(struct pt_regs));
	} else {
		/* User thread */

		/* initialize the context with the same as the one currently running */
		*uregs = *current_pt_regs();

		uregs->regs[2] = 0; /* child gets zero as return value */

		/* setup user stack if specified */
		if (usp)
			uregs->regs[29] = usp;

		/* tell ret_from_fork it's a user thread */
		kregs->regs[16] = 0;

		/* set a new tls area if required (as explained in alpha, if
		 * CLONE_SETTLS isn't set, the whole thread_info of the forking
		 * task was cloned, so the child inherits of it's parent tls
		 * area anyway) */
		if (unlikely(clone_flags & CLONE_SETTLS))
			/* new tls area is the 4th argument (see CLONE_BACKWARDS) */
			task_thread_info(p)->tp_value = uregs->regs[7];
	}

	/* when doing the _switch to this newly created thread, the stack
	 * pointer will be restored as pointing on kregs and it will return to
	 * ret_from_fork */
	kregs->regs[29] = top_of_kstack;
	kregs->regs[31] = (unsigned long)ret_from_fork;

	task_thread_info(p)->ksp = (unsigned long)kregs;

	return 0;
}

void exit_thread(void)
{
}

void flush_thread(void)
{
}

unsigned long thread_saved_pc(struct task_struct *tsk)
{
	return task_pt_regs(tsk)->cp0_epc;
}

unsigned long get_wchan(struct task_struct *task)
{
	if (!task || task == current || task->state == TASK_RUNNING)
		return 0;

	if (!task_stack_page(task))
		return 0;

	return thread_saved_pc(task);
}

/*
 * Context switching between a 'prev' (current running) task and a 'next' task
 */
extern asmlinkage struct thread_info *__asm_switch_to(struct thread_info *prev_ti,
		struct thread_info *next_ti);

struct task_struct *__switch_to(struct task_struct *prev,
		struct task_struct *next)
{
	struct thread_info *next_ti, *prev_ti, *last_ti;
	unsigned long flags;

	/* get both thread_info structures at the bottom of respective kstacks */
	next_ti = task_thread_info(next);
	prev_ti = task_thread_info(prev);

	/*
	 * XXX: MIPS does context switching without disabling interrupts. But
	 * for now, let's disable them before switching to avoid any further
	 * trouble.
	 */
	local_irq_save(flags);

	/* change the current thread_info ($28, aka $gp) */
	current_thread_info() = next_ti;

	/* save the newly current thread_info for this cpu. We need it at
	 * user->kernel transistions, especially to find the corresponding
	 * kernel stack (thread_info->ksp).
	 * Note: ksp is used only for user->kernel transistion */
	write_c0_tccontext(current_thread_info());

	/* performs the actual switch:
	 * - before the switch, 'prev' is the currently running task, 'next' is
	 *   the task to schedule
	 * - after the switch, 'prev' is the (newly) running task (because at
	 *   the moment of being descheduled, it was the actual 'prev'), 'next'
	 *   is not pertinent, and 'last' is the former 'prev' from before the
	 *   switch (ie the task that just got descheduled) */
	last_ti = __asm_switch_to(prev_ti, next_ti);

	local_irq_restore(flags);

	return last_ti->task;
}

/*
 * Register dumping
 */

void elf_dump_regs(elf_greg_t *gp, struct pt_regs *regs)
{
}

int dump_task_regs(struct task_struct *tsk, elf_gregset_t *gre)
{
	elf_dump_regs(*gre, task_pt_regs(tsk));
	return 1;
}

int dump_task_fpu(struct task_struct *tsk, elf_fpregset_t *fpr)
{
	return 1;
}
