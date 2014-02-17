#ifndef _ASM_TSAR_MIPS32C0_REGS_H
#define _ASM_TSAR_MIPS32C0_REGS_H

#include <linux/const.h>

/*
 * Coprocessor 0 register names
 */
#define CP0_INDEX	$0
#define CP0_RANDOM	$1
#define CP0_ENTRYLO0	$2
#define CP0_ENTRYLO1	$3
#define CP0_CONF	$3
#define CP0_CONTEXT	$4
#define CP0_USERLOCAL	$4 // (4, 2)
#define CP0_PAGEMASK	$5
#define CP0_WIRED	$6
#define CP0_HWRENA	$7
#define CP0_BADVADDR	$8
#define CP0_COUNT	$9
#define CP0_ENTRYHI	$10
#define CP0_COMPARE	$11
#define CP0_STATUS	$12
#define CP0_CAUSE	$13
#define CP0_EPC		$14
#define CP0_PRID	$15
#define CP0_EBASE	$15 // (15, 1)
#define CP0_CONFIG	$16
#define CP0_LLADDR	$17
#define CP0_WATCHLO	$18
#define CP0_WATCHHI	$19
#define CP0_XCONTEXT	$20
#define CP0_FRAMEMASK	$21
#define CP0_DIAGNOSTIC	$22
#define CP0_DEBUG	$23
#define CP0_DEPC	$24
#define CP0_PERFORMANCE	$25
#define CP0_ECC		$26
#define CP0_CACHEERR	$27
#define CP0_TAGLO	$28
#define CP0_TAGHI	$29
#define CP0_ERROREPC	$30
#define CP0_DESAVE	$31

/*
 * Bitfields
 */

/* cp0 hwrena register */
#define HWRENAB_CPUNUM	0
#define HWRENAF_CPUNUM	(_AC(1, UL) << HWRENAB_CPUNUM)
#define HWRENAB_CC	2
#define HWRENAF_CC	(_AC(1, UL) << HWRENAB_CC)
#define HWRENAB_CCRES	3
#define HWRENAF_CCRES	(_AC(1, UL) << HWRENAB_CCRES)
#define HWRENAB_ULR	29
#define HWRENAF_ULR	(_AC(1, UL) << HWRENAB_ULR)

/* cp0 status register */
#define ST0_IE	0x00000001

#define ST0_EXL	0x00000002
#define ST0_ERL	0x00000004

#define ST0_KSU	0x00000018
# define ST0_KSU_USER		0x00000010
# define ST0_KSU_SUPERVISOR	0x00000008
# define ST0_KSU_KERNEL		0x00000000

#define ST0_IM	0x0000ff00

#define ST0_CU	0xf0000000
#define ST0_CU0	0x10000000
#define ST0_CU1	0x20000000
#define ST0_CU2	0x40000000
#define ST0_CU3	0x80000000

/* cp0 cause register */
#define CAUSEB_EXCCODE	2 // Exception code
#define CAUSEF_EXCCODE	(_AC(31, UL) << 2)

#define	 CAUSEB_IP	8 // Interrupt Pending
#define	 CAUSEF_IP	(_AC(255, UL) <<  CAUSEB_IP)
#define	 CAUSEB_CE	28 // Coprocessor Unusable number
#define	 CAUSEF_CE	(_AC(3, UL) <<  CAUSEB_CE)
#define	 CAUSEB_BD	31 // Branch Delay slot
#define	 CAUSEF_BD	(_AC(1, UL) <<  CAUSEB_CE)


/* cp0 cause register, exception codes */
#define	CAUSE_EXCCODE_INT	0
#define	CAUSE_EXCCODE_ADEL	4
#define	CAUSE_EXCCODE_ADES	5
#define	CAUSE_EXCCODE_IBE	6
#define	CAUSE_EXCCODE_DBE	7
#define	CAUSE_EXCCODE_SYS	8
#define	CAUSE_EXCCODE_BP	9
#define	CAUSE_EXCCODE_RI	10
#define	CAUSE_EXCCODE_CPU	11
#define	CAUSE_EXCCODE_OV	12
#define	CAUSE_EXCCODE_TR	13
#define	CAUSE_EXCCODE_FPE	15

#endif /* _ASM_TSAR_MIPS32C0_REGS_H */
