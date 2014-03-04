#ifndef _ASM_TSAR_TIMEX_H
#define _ASM_TSAR_TIMEX_H

#include <asm/mips32c0.h>

/* the register for cycle counting is 32 bits */
typedef unsigned long cycles_t;

static inline cycles_t get_cycles(void)
{
	/* returns count register in coprocessor0 */
	return read_c0_count();
}


#define ARCH_HAS_READ_CURRENT_TIMER

static inline int read_current_timer(unsigned long *timer_value)
{
	*timer_value = get_cycles();
	return 0;
}

#endif /* _ASM_TSAR_TIMEX_H */
