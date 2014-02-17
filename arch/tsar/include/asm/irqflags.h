#ifndef _ASM_TSAR_IRQFLAGS_H
#define _ASM_TSAR_IRQFLAGS_H

#ifndef __ASSEMBLY__

#include <asm/mips32c0.h>

static inline unsigned long arch_local_save_flags(void)
{
	return read_c0_status() & (ST0_IE);
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	write_c0_status((read_c0_status() & ~ST0_IE) | (flags & ST0_IE));
}

#include <asm-generic/irqflags.h>

#endif /* __ASSEMBLY__ */

#endif /*_ASM_TSAR_IRQFLAGS_H */
