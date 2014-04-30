#ifndef _ASM_TSAR_BRANCH_H
#define _ASM_TSAR_BRANCH_H

#include <asm/ptrace.h>

static inline unsigned long exception_epc(struct pt_regs *regs)
{
	/* if the instruction is not a delayed slot,
	 * then the epc value is correct */
	if (likely(!cause_bd(regs->cp0_cause)))
		return regs->cp0_epc;

	/* otherwise it is the next instruction */
	return regs->cp0_epc + 4;
}

#endif /* _ASM_TSAR_BRANCH_H */
