#ifndef _ASM_TSAR_MMUC2_H
#define _ASM_TSAR_MMUC2_H

#include <asm/mmuc2_regs.h>

/*
 * Macros to access coprocessor 2
 */

#define __READ_C2_REG(_res, _src)         \
	__asm__ __volatile__(             \
			"mfc2 %0, " #_src \
			: "=r" (_res)     \
			)

#define __WRITE_C2_REG(_val, _src)                      \
	__asm__ __volatile__(                           \
			"mtc2 %z0, "#_src               \
			:: "Jr" ((unsigned long)(_val)) \
			)

#ifndef __ASSEMBLY__

/* macros for C code */

#define __read_c2_reg(reg)        \
	({ unsigned long res;     \
	 __READ_C2_REG(res, reg); \
	 res;                     \
	 })

#define __write_c2_reg(val, reg) \
	do { __WRITE_C2_REG(val, reg); } while(0)

/*
 * read/write macros for MMU_C2 registers
 */
#define read_c2_mode()		__read_c2_reg(MMU_MODE)
#define write_c2_mode(val)	__write_c2_reg(val, MMU_MODE)

#define read_c2_ptpr()		__read_c2_reg(MMU_PTPR)
#define write_c2_ptpr(val)	__write_c2_reg(val, MMU_PTPR)

#define read_c2_ietr()		__read_c2_reg(MMU_IETR)
#define read_c2_detr()		__read_c2_reg(MMU_DETR)

#define read_c2_ibvar()		__read_c2_reg(MMU_IBVAR)
#define read_c2_dbvar()		__read_c2_reg(MMU_DBVAR)

#define write_c2_dpaext(val)	__write_c2_reg(val, MMU_DATA_PADDR_EXT)

/* set, clear macros */
#define set_c2_mode(set)		\
	({ unsigned long _old, _new;	\
	 _old = read_c2_mode();		\
	 _new = _old | (set);		\
	 write_c2_mode(_new);		\
	 _old;				\
	 })

#define clear_c2_mode(set)		\
	({ unsigned long _old, _new;	\
	 _old = read_c2_mode();		\
	 _new = _old & ~(set);		\
	 write_c2_mode(_new);		\
	 _old;				\
	 })

#endif /* ! __ASSEMBLY__ */

#endif /* _ASM_TSAR_MMUC2_H */
