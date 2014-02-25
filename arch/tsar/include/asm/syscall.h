/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#ifndef _ASM_TSAR_SYSCALL_H
#define _ASM_TSAR_SYSCALL_H

static inline int
syscall_get_nr(struct task_struct *task, struct pt_regs *regs)
{
	/* PT_R0 keeps track of the syscall number + 1*/
	return regs->regs[0] - 1;
}

static inline void
syscall_rollback(struct task_struct *task, struct pt_regs *regs)
{
	regs->regs[2] = regs->regs[0] - 1;
}

static inline long
syscall_get_error(struct task_struct *task, struct pt_regs *regs)
{
	return IS_ERR_VALUE(regs->regs[2]) ? regs->regs[2] : 0;
}

static inline long
syscall_get_return_value(struct task_struct *task, struct pt_regs *regs)
{
	return regs->regs[2];
}

static inline void
syscall_set_return_value(struct task_struct *task, struct pt_regs *regs,
			 int error, long val)
{
	regs->regs[2] = (long) error ?: val;
}

static inline void
syscall_get_arguments(struct task_struct *task, struct pt_regs *regs,
		      unsigned int i, unsigned int n, unsigned long *args)
{
	BUG_ON(i + n > 6);

	memcpy(args, &regs->regs[4 + i], n * sizeof(args[0]));
}

static inline void
syscall_set_arguments(struct task_struct *task, struct pt_regs *regs,
		      unsigned int i, unsigned int n, const unsigned long *args)
{
	BUG_ON(i + n > 6);

	memcpy(&regs->regs[4 + i], args, n * sizeof(args[0]));
}

#endif /* _ASM_TSAR_SYSCALL_H */
