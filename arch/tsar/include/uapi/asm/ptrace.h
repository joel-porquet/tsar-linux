#ifndef _UAPI_ASM_TSAR_PTRACE_H
#define _UAPI_ASM_TSAR_PTRACE_H

#ifndef __ASSEMBLY__

/*
 * Definition of the regset that is exchanged with the userspace
 *
 * Note: we don't use the name pt_regs to avoid confusion with the regset that
 * is used only in kernelspace.
 *
 * Also, let's make it compatible with the MIPS elf_greg_t array
 */
struct user_regs_struct {
	unsigned long __pad[6];

	/* main processor registers */
	unsigned long regs[32];

	/* special registers */
	unsigned long lo;
	unsigned long hi;

	unsigned long cp0_epc;
	unsigned long cp0_badvaddr;
	unsigned long cp0_status;
	unsigned long cp0_cause;

	unsigned long __unused0;
} __attribute__ ((aligned (8)));

#endif /* __ASSEMBLY__ */

#endif /* _UAPI_ASM_TSAR_PTRACE_H */
