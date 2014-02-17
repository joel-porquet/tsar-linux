/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/elf.h>
#include <linux/ptrace.h>
#include <linux/regset.h>
#include <linux/sched.h>

#include <asm/elf.h>
#include <asm/ptrace.h>

/*
 * Copy the thread state to a regset that can be interpreted by userspace
 */
static int genregs_get(struct task_struct *target,
		const struct user_regset *regset,
		unsigned int pos, unsigned int count,
		void *kbuf, void __user *ubuf)
{
	const struct pt_regs *regs = task_pt_regs(target);
	int ret;

	/* skip 7 * sizeof(unsigned long): __pad[6] and regs[0] */
	ret = user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf,
			0, offsetof(struct user_regs_struct, regs));

	/* regs[1] to regs[31] and special registers lo, hi, cp0_epc,
	 * cp0_badvaddr, cp0_status, cp0_cause */
	if (!ret)
		ret = user_regset_copyout(&pos, &count, &kbuf, &ubuf,
				regs->regs,
				offsetof(struct user_regs_struct, regs),
				offsetof(struct user_regs_struct, cp0_cause));

	/* fill the rest with zero if necessary */
	if (!ret)
		ret = user_regset_copyout_zero(&pos, &count, &kbuf, &ubuf,
				sizeof(struct user_regs_struct), -1);

	return ret;
}

/*
 * Set the thread state from a regset passed via ptrace
 */
static int genregs_set(struct task_struct *target,
		const struct user_regset *regset,
		unsigned int pos, unsigned int count,
		const void *kbuf, const void __user *ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	int ret;

	/* ignore 7 * sizeof(unsigned long): __pad[6] and regs[0] */
	ret = user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf,
			0, offsetof(struct user_regs_struct, regs));

	/* regs[1] to regs[31] and special registers lo, hi, cp0_epc */
	if (!ret)
		ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				regs->regs,
				offsetof(struct user_regs_struct, regs),
				offsetof(struct user_regs_struct, cp0_epc));

	/* ignore the rest: cp0_badvaddr, cp0_status, cp0_cause, etc. */
	if (!ret)
		ret = user_regset_copyin_ignore(&pos, &count, &kbuf, &ubuf,
				sizeof(struct user_regs_struct), -1);

	return ret;
}

/*
 * Define the register sets available on TSAR under Linux
 */
enum tsar_regset {
	REGSET_GENERAL,
};

static const struct user_regset tsar_regsets[] = {
	[REGSET_GENERAL] = {
		.core_note_type = NT_PRSTATUS,
		.n = ELF_NGREG,
		.size = sizeof(long),
		.align = sizeof(long),
		.get = genregs_get,
		.set = genregs_set,
	},
};

static const struct user_regset_view user_tsar_native_view = {
	.name = "tsar",
	.e_machine = ELF_ARCH,
	.regsets = tsar_regsets,
	.n = ARRAY_SIZE(tsar_regsets),
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_tsar_native_view;
}

void ptrace_disable(struct task_struct *child)
{
	pr_debug("ptrace_disable(): TODO\n");

	user_disable_single_step(child);
	clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
}

long arch_ptrace(struct task_struct *child, long request, unsigned long addr,
		unsigned long data)
{
	int ret;

	switch (request) {
		default:
			ret = ptrace_request(child, request, addr, data);
			break;
	}

	return ret;
}

