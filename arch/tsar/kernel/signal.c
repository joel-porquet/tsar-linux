/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/tracehook.h>

#include <asm/ptrace.h>
#include <asm/thread_info.h>
#include <asm/uaccess.h>
#include <asm/ucontext.h>
#include <asm/unistd.h>

struct rt_sigframe {
	struct siginfo info;
	struct ucontext uc;
	unsigned long retcode[2];
};

static int restore_sigcontext(struct pt_regs *regs, struct sigcontext *sc)
{
	int err = 0;
	int i;

	/* always make any pending restarted system call return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	/* restore the regs */
	for (i = 1; i < 32; i++)
		err|= __get_user(regs->regs[i], &sc->regs.regs[i]);

	err|= __get_user(regs->hi, &sc->regs.hi);
	err|= __get_user(regs->lo, &sc->regs.lo);
	err|= __get_user(regs->cp0_epc, &sc->regs.cp0_epc);

	return err;
}

static int setup_sigcontext(struct sigcontext __user *sc, struct pt_regs *regs,
		unsigned long mask)
{
	int err = 0;
	int i;

	/* copy the regs */
	err |= __put_user(0, &sc->regs.regs[0]);
	for (i = 1; i < 32; i++)
		err|= __put_user(regs->regs[i], &sc->regs.regs[i]);

	err|= __put_user(regs->hi, &sc->regs.hi);
	err|= __put_user(regs->lo, &sc->regs.lo);
	err|= __put_user(regs->cp0_epc, &sc->regs.cp0_epc);

	/* and the mask */
	err |= __put_user(mask, &sc->oldmask);

	return err;
}

static inline unsigned long align_sigframe(unsigned long sp)
{
	return sp & ~7UL;
}

static void __user *get_sigframe(struct k_sigaction *ka,
		struct pt_regs *regs, size_t frame_size)
{
	unsigned long sp;
	int onsigstack;

	/* default is to use the regular stack */
	sp = regs->regs[29];

	onsigstack = on_sig_stack(sp);

	/* make room for a stackframe */
	sp -= 32;

	/* This is the X/Open sanctioned signal stack switching.  */
	if ((ka->sa.sa_flags & SA_ONSTACK) && !onsigstack) {
		if (current->sas_ss_size)
			sp = current->sas_ss_sp + current->sas_ss_size;
	}

	sp = align_sigframe(sp - frame_size);

	/* avoid overflowing the alternate signal stack
	 * in which case make it die with SIGSEGV */
	if (onsigstack && !likely(on_sig_stack(sp)))
		return (void __user *)-1L;

	return (void __user *)sp;

}

static int setup_rt_frame(struct ksignal *ksig, struct pt_regs *regs)
{
	sigset_t *set = sigmask_to_save();
	struct rt_sigframe __user *frame;
	int err = 0;

	frame = get_sigframe(&ksig->ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		return -EFAULT;

	/* create siginfo */
	err |= copy_siginfo_to_user(&frame->info, &ksig->info);

	/* create ucontext */
	err |= __clear_user(&frame->uc, offsetof(struct ucontext, uc_mcontext));
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(NULL, &frame->uc.uc_link);
	err |= __save_altstack(&frame->uc.uc_stack, regs->regs[29]);
	err |= setup_sigcontext(&frame->uc.uc_mcontext, regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		return -EFAULT;

	/* setup the trampoline:
	 * 	li	v0, __NR_rt_sigreturn
	 * 	syscall
	 */
	err |= __put_user(0x24020000 + __NR_rt_sigreturn, &frame->retcode[0]);
	err |= __put_user(0x0000000c, &frame->retcode[1]);

	if (err)
		return -EFAULT;

	/* set up registers for signal handler */

	/* arguments: 1/ signal number, 2/ siginfo_t, 3/ ucontext */
	regs->regs[4] = ksig->sig;
	regs->regs[5] = (unsigned long)&frame->info;
	regs->regs[6] = (unsigned long)&frame->uc;
	/* sp points on struct rt_sigframe */
	regs->regs[29] = (unsigned long)frame;
	/* trampoline code for after returning from signal handler */
	regs->regs[31] = (unsigned long)frame->retcode;
	/* signal handler (PT_25 is the pic register) */
	regs->cp0_epc = regs->regs[25] = (unsigned long)ksig->ka.sa.sa_handler;

	return 0;
}

static void handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	bool failed;

	/* did we come from a syscall? */
	if (regs->regs[0] > 0) {
		/* does the syscall need to be restarted? */
		switch (regs->regs[2]) {
			case -ERESTART_RESTARTBLOCK:
			case -ERESTARTNOHAND:
				regs->regs[2] = -EINTR;
				break;
			case -ERESTARTSYS:
				if (!(ksig->ka.sa.sa_flags & SA_RESTART)) {
					regs->regs[2] = -EINTR;
					break;
				}
			case -ERESTARTNOINTR:
				regs->regs[2] = regs->regs[0] - 1;
				regs->cp0_epc -= 4;
				break;
		}
		regs->regs[0] = 0;
	}

	failed = (setup_rt_frame(ksig, regs) < 0);

	signal_setup_done(failed, ksig, 0);
}

static void do_signal(struct pt_regs *regs)
{
	struct ksignal ksig;

	if (get_signal(&ksig)) {
		/* deliver the signal */
		handle_signal(&ksig, regs);
		return;
	}

	/* now we're in the case no handler was defined for the signal */

	/* did we come from a syscall?
	 * if so, PT_RO = syscall_nr + 1 (which is necessarily > 0) */
	if (regs->regs[0] > 0) {
		/* does the syscall need to be restarted? */
		switch (regs->regs[2]) {
			case -ERESTARTNOHAND:
			case -ERESTARTSYS:
			case -ERESTARTNOINTR:
				/* reset it to its original value and rewind
				 * PC */
				regs->regs[2] = regs->regs[0] - 1;
				regs->cp0_epc -= 4;
				break;
			case -ERESTART_RESTARTBLOCK:
				/* restart by calling a special syscall */
				regs->regs[2] = __NR_restart_syscall;
				regs->cp0_epc -= 4;
				break;
		}
		regs->regs[0] = 0;
	}

	restore_saved_sigmask();
}

asmlinkage void do_notify_resume(struct pt_regs *regs)
{
	/* XXX: some (mips, arm, re-enable interrupts here... */

	if (test_thread_flag(TIF_SIGPENDING))
		do_signal(regs);

	if (test_and_clear_thread_flag(TIF_NOTIFY_RESUME))
		tracehook_notify_resume(regs);
}

asmlinkage long _sys_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe __user *frame = (struct rt_sigframe __user *)regs->regs[29];
	sigset_t set;

	/* check the user didn't mess the frame */
	if ((unsigned long)frame != align_sigframe((unsigned long)frame))
		goto badframe;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	set_current_blocked(&set);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;

	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;

	return regs->regs[2];

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

