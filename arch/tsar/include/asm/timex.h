#ifndef __ASM_TSAR_TIMEX_H
#define __ASM_TSAR_TIMEX_H

/* avoid declaration of get_cycles in asm-generic/timex.h> */
#define get_cycles get_cycles

#include <asm-generic/timex.h> /* cycles_t */

#include <asm/mips32c0.h>

static inline cycles_t get_cycles(void)
{
	return read_c0_count();
}

#define ARCH_HAS_READ_CURRENT_TIMER

#endif
