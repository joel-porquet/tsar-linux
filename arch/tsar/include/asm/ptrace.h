#ifndef _ASM_TSAR_PTRACE_H
#define _ASM_TSAR_PTRACE_H

#include <linux/compiler.h>

#include <asm/cache.h>
#include <asm/mips32c0_regs.h>

#include <uapi/asm/ptrace.h>

/*
 * This file defines what is a context, called pt_regs, and how it is arranged
 * on the kernel stack, during a system call or other kernel entry.
 *
 * It is (quite) unrelated to the pt_regs structure that is defined in
 * uapi/asm/ptrace and exported (is it really?) to userspace
 */

#ifndef __ASSEMBLY__

/*
 * Let's try to use something as close as the one defined for MIPS
 *
 * Aligned the structure on a cache line
 */

struct pt_regs {
	/* room for 8 arguments (in case of syscall, we can
	 * have up to 6 arguments, so make lot of room on the
	 * ksp for that) */
	unsigned long args[8];

	/* main processor registers */
	unsigned long regs[32];

	/* special registers */
	unsigned long lo;
	unsigned long hi;

	unsigned long cp0_epc;
	unsigned long cp0_badvaddr;
	unsigned long cp0_status;
	unsigned long cp0_cause;
} __aligned(L1_CACHE_BYTES);

#define user_mode(regs) \
	(((regs)->cp0_status & ST0_KSU) == ST0_KSU_USER)

/*
 * redefine current_pt_regs() to be faster
 */
#define current_pt_regs()                                              \
({                                                                     \
	 unsigned long sp = (unsigned long)__builtin_frame_address(0); \
	 (struct pt_regs *)((sp | (THREAD_SIZE - 1)) + 1) - 1;         \
 })

#define GET_IP(regs)	((regs)->cp0_epc)
#define GET_USP(regs)	((regs)->regs[29])
#define GET_FP(regs)	((regs)->regs[30])

#include <asm-generic/ptrace.h>

#endif /* __ASSEMBLY__ */

#endif /* _ASM_TSAR_PTRACE_H */
