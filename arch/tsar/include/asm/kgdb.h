#ifndef _ASM_TSAR_KGDB_H
#define _ASM_TSAR_KGDB_H

/* no need to flush the cache, we are memory coherent */
#define CACHE_FLUSH_IS_SAFE 0

/* follow what the others do (i.e. at least 2 * NUMREGBYTES but more if
 * possible) */
#define BUFMAX 2048

/*
 * gdb expects the following register layout:
 * - 32 general purpose registers: r0-r31
 * - 6 extra registers: sr, lo, hi, bad, cause, pc
 */
#define _GP_REGS 32
#define _EX_REGS 6

#define DBG_MAX_REG_NUM (_GP_REGS + _EX_REGS)
#define NUMREGBYTES	(DBG_MAX_REG_NUM * 4)

/* break is a 32-bit instruction */
#define BREAK_INSTR_SIZE 4
static inline void arch_kgdb_breakpoint(void)
{
	__asm__ __volatile__("break");
}

#endif /* _ASM_TSAR_KGDB_H */
