#ifndef _ASM_TSAR_MIPS32C0_H
#define _ASM_TSAR_MIPS32C0_H

#include <asm/mips32c0_regs.h>

/*
 * Macros to access coprocessor 0
 */

#define __READ_C0_REG_SEL(_res, _src, _sel)       \
	__asm__ __volatile__(                     \
			"mfc0 %0, "#_src", "#_sel \
			: "=r" (_res)             \
			)

#define __WRITE_C0_REG_SEL(_val, _src, _sel)            \
	__asm__ __volatile__(                           \
			"mtc0 %z0, "#_src", "#_sel      \
			:: "Jr" ((unsigned long)(_val)) \
			)

#ifndef __ASSEMBLY__

/* macros for C code */

#define __read_c0_reg_sel(reg, sel)        \
	({ unsigned long res;              \
	 __READ_C0_REG_SEL(res, reg, sel); \
	 res;                              \
	 })

#define __write_c0_reg_sel(val, reg, sel) \
	do { __WRITE_C0_REG_SEL((val), reg, sel); } while(0)

/* read/write macros */
#define read_c0_userlocal()	__read_c0_reg_sel(CP0_USERLOCAL, 2)
#define write_c0_userlocal(val)	__write_c0_reg_sel((val), CP0_USERLOCAL, 2)

#define read_c0_hwrena()	__read_c0_reg_sel(CP0_HWRENA, 0)
#define write_c0_hwrena(val)	__write_c0_reg_sel((val), CP0_HWRENA, 0)

#define read_c0_count()		__read_c0_reg_sel(CP0_COUNT, 0)
#define write_c0_count(val) 	__write_c0_reg_sel((val), CP0_COUNT, 0)

#define read_c0_status()	__read_c0_reg_sel(CP0_STATUS, 0)
#define write_c0_status(val) 	__write_c0_reg_sel((val), CP0_STATUS, 0)

#define read_c0_cause()		__read_c0_reg_sel(CP0_CAUSE, 0)
#define write_c0_cause(val) 	__write_c0_reg_sel((val), CP0_CAUSE, 0)

#define read_c0_prid()		__read_c0_reg_sel(CP0_PRID, 0)

#define read_c0_ebase()		__read_c0_reg_sel(CP0_EBASE, 1)
#define write_c0_ebase(val)	__write_c0_reg_sel((val), CP0_EBASE, 1)

#define read_c0_hwcpuid()	(read_c0_ebase() & EBASE_CPUHWID)

#define write_c0_tccontext(val)	__write_c0_reg_sel((val), CP0_TCCONTEXT, 5)

/* set, clear, change macros */
#define set_c0_status(set)		\
	({ unsigned long _old, _new;	\
	 _old = read_c0_status();	\
	 _new = _old | (set);		\
	 write_c0_status(_new);		\
	 _old;				\
	 })

#define clear_c0_status(set)		\
	({ unsigned long _old, _new;	\
	 _old = read_c0_status();	\
	 _new = _old & ~(set);		\
	 write_c0_status(_new);		\
	 _old;				\
	 })

#endif /* ! __ASSEMBLY__ */

#endif /* _ASM_TSAR_MIPS32C0_H */
