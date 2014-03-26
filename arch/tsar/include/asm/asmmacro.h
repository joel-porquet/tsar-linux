#ifndef _ASM_TSAR_ASMMACRO_H
#define _ASM_TSAR_ASMMACRO_H

#include <generated/asm-offsets.h>

#include <asm/mips32c0_regs.h>

#ifdef __ASSEMBLY__

/*
 * SETUP_KSTACK macro
 * Given a certain global pointer (that points to a thread_info structure),
 * compute the kernel stack pointer accordingly (i.e.  after a full pt_regs)
 * and save in the thread_info structure.
 */

#define SETUP_KSTACK                       \
	li	sp, THREAD_SIZE - PT_SIZE ;\
	addu	sp, sp, gp                ;\
	sw	sp, TI_KSP(gp)            ;


/*
 * GET_SAVED_KSP macro
 * must return the saved KSP of the current cpu in k1
 */

#ifdef CONFIG_SMP

#define GET_SAVED_KSP                        \
	mfc0	k1, CP0_EBASE, 1            ;\
	andi	k1, EBASE_CPUHWID           ;\
	sll	k1, 2                       ;\
	la	k0, current_thread_info_set ;\
	add	k0, k1                      ;\
	lw	k0, (k0)                    ;\
	lw	k1, TI_KSP(k0)              ;

#else /* !CONFIG_SMP */

#define GET_SAVED_KSP                        \
	la	k0, current_thread_info_set ;\
	lw	k0, (k0)                    ;\
	lw	k1, TI_KSP(k0)              ;

#endif /* CONFIG_SMP */


/*
 * IRQ enabling/disabling
 */

#define LOCAL_IRQ_DISABLE       \
	mfc0	t0, CP0_STATUS ;\
	ori	t0, t0, 1      ;\
	xori	t0, t0, 1      ;\
	mtc0	t0, CP0_STATUS ;

#define LOCAL_IRQ_ENABLE        \
	mfc0	t0, CP0_STATUS ;\
	ori	t0, t0, 1      ;\
	mtc0	t0, CP0_STATUS ;


/*
 * State switching: CLI/STI/KMODE
 */

#define STATE_MASK_NOIE (ST0_KSU | ST0_EXL | ST0_ERL)
#define STATE_MASK_IE (STATE_MASK_NOIE | ST0_IE)

/* CLI: switch to pure kernel mode and disable interruptions */
#define CLI                        \
	mfc0	t0, CP0_STATUS    ;\
	li	t1, STATE_MASK_IE ;\
	or	t0, t1            ;\
	xori	t0, STATE_MASK_IE ;\
	mtc0	t0, CP0_STATUS    ;

/* STI: switch to pure kernel mode and enable interruptions */
#define STI                          \
	mfc0	t0, CP0_STATUS      ;\
	li	t1, STATE_MASK_IE   ;\
	or	t0, t1              ;\
	xori	t0, STATE_MASK_NOIE ;\
	mtc0	t0, CP0_STATUS      ;

/* KMODE: switch to pure kernel mode but don't touch interruptions */
#define KMODE                        \
	mfc0	t0, CP0_STATUS      ;\
	li	t1, STATE_MASK_NOIE ;\
	or	t0, t1              ;\
	xori	t0, STATE_MASK_NOIE ;\
	mtc0	t0, CP0_STATUS      ;

/*
 * Context saving/restoring
 */

#define SAVE_ALL                                                 \
	.set push                                               ;\
	.set noat                                               ;\
	.set noreorder                                          ;\
	mfc0	k0, CP0_STATUS                                  ;\
	sll	k0, 27	/* extract UM bit */                    ;\
	bgtz	k0, 2f	/* branch if already in kernel mode */  ;\
	move	k1, sp	/* store kernel sp in k1 */             ;\
	1:                                                      ;\
	/* user mode */                                         ;\
	GET_SAVED_KSP                                           ;\
	2:                                                      ;\
	/* kernel mode: now k1 contains a valid ptr on ksp */   ;\
	/* make room for pt_regs and a stackframe */            ;\
	subu	k1, PT_SIZE                                     ;\
	move	k0, sp                                          ;\
	move	sp, k1                                          ;\
	sw	k0, PT_R29(sp) /* save old stack right away */  ;\
	sw	$0, PT_R0(sp) /* save 0 by default */           ;\
	sw	$1, PT_R1(sp)                                   ;\
	sw	$2, PT_R2(sp)                                   ;\
	sw	$3, PT_R3(sp)                                   ;\
	sw	$4, PT_R4(sp)                                   ;\
	sw	$5, PT_R5(sp)                                   ;\
	sw	$6, PT_R6(sp)                                   ;\
	sw	$7, PT_R7(sp)                                   ;\
	sw	$8, PT_R8(sp)                                   ;\
	sw	$9, PT_R9(sp)                                   ;\
	sw	$10, PT_R10(sp)                                 ;\
	sw	$11, PT_R11(sp)                                 ;\
	sw	$12, PT_R12(sp)                                 ;\
	sw	$13, PT_R13(sp)                                 ;\
	sw	$14, PT_R14(sp)                                 ;\
	sw	$15, PT_R15(sp)                                 ;\
	sw	$16, PT_R16(sp)                                 ;\
	sw	$17, PT_R17(sp)                                 ;\
	sw	$18, PT_R18(sp)                                 ;\
	sw	$19, PT_R19(sp)                                 ;\
	sw	$20, PT_R20(sp)                                 ;\
	sw	$21, PT_R21(sp)                                 ;\
	sw	$22, PT_R22(sp)                                 ;\
	sw	$23, PT_R23(sp)                                 ;\
	sw	$24, PT_R24(sp)                                 ;\
	sw	$25, PT_R25(sp)                                 ;\
	sw	$28, PT_R28(sp)                                 ;\
	sw	$30, PT_R30(sp)                                 ;\
	sw	$31, PT_R31(sp)                                 ;\
	mfhi	$3                                              ;\
	sw	$3, PT_HI(sp)                                   ;\
	mflo	$3                                              ;\
	sw	$3, PT_LO(sp)                                   ;\
	mfc0	$3, CP0_STATUS                                  ;\
	sw	$3, PT_STATUS(sp)                               ;\
	mfc0	$3, CP0_CAUSE                                   ;\
	sw	$3, PT_CAUSE(sp)                                ;\
	mfc0	$3, CP0_EPC                                     ;\
	sw	$3, PT_EPC(sp)                                  ;\
	/* set direct ptr on current_thread_info */             ;\
	ori	$28, sp, THREAD_MASK                            ;\
	xori	$28, THREAD_MASK                                ;\
	.set pop                                                ;

#define RESTORE_ALL_AND_RET                                      \
	.set push                                               ;\
	.set noat                                               ;\
	.set noreorder                                          ;\
	lw	$3, PT_HI(sp)                                   ;\
	mthi	$3                                              ;\
	lw	$3, PT_LO(sp)                                   ;\
	mtlo	$3                                              ;\
	lw	$3, PT_STATUS(sp)                               ;\
	mtc0	$3, CP0_STATUS                                  ;\
	lw	$3, PT_EPC(sp)                                  ;\
	mtc0	$3, CP0_EPC                                     ;\
	lw	$1, PT_R1(sp)                                   ;\
	lw	$2, PT_R2(sp)                                   ;\
	lw	$3, PT_R3(sp)                                   ;\
	lw	$4, PT_R4(sp)                                   ;\
	lw	$5, PT_R5(sp)                                   ;\
	lw	$6, PT_R6(sp)                                   ;\
	lw	$7, PT_R7(sp)                                   ;\
	lw	$8, PT_R8(sp)                                   ;\
	lw	$9, PT_R9(sp)                                   ;\
	lw	$10, PT_R10(sp)                                 ;\
	lw	$11, PT_R11(sp)                                 ;\
	lw	$12, PT_R12(sp)                                 ;\
	lw	$13, PT_R13(sp)                                 ;\
	lw	$14, PT_R14(sp)                                 ;\
	lw	$15, PT_R15(sp)                                 ;\
	lw	$16, PT_R16(sp)                                 ;\
	lw	$17, PT_R17(sp)                                 ;\
	lw	$18, PT_R18(sp)                                 ;\
	lw	$19, PT_R19(sp)                                 ;\
	lw	$20, PT_R20(sp)                                 ;\
	lw	$21, PT_R21(sp)                                 ;\
	lw	$22, PT_R22(sp)                                 ;\
	lw	$23, PT_R23(sp)                                 ;\
	lw	$24, PT_R24(sp)                                 ;\
	lw	$25, PT_R25(sp)                                 ;\
	lw	$28, PT_R28(sp)                                 ;\
	lw	$30, PT_R30(sp)                                 ;\
	lw	$31, PT_R31(sp)                                 ;\
	lw	$29, PT_R29(sp)                                 ;\
	eret                                                    ;\
	.set pop                                                ;

#endif /* __ASSEMBLY__ */

#endif /* _ASM_TSAR_ASMMACRO_H */
