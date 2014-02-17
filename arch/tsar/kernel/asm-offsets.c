/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/stddef.h>
#include <linux/kbuild.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>

void output_ptreg_defines(void)
{
	COMMENT("TSARMIPS pt_regs offsets.");
	OFFSET(PT_R0, pt_regs, regs[0]);
	OFFSET(PT_R1, pt_regs, regs[1]);
	OFFSET(PT_R2, pt_regs, regs[2]);
	OFFSET(PT_R3, pt_regs, regs[3]);
	OFFSET(PT_R4, pt_regs, regs[4]);
	OFFSET(PT_R5, pt_regs, regs[5]);
	OFFSET(PT_R6, pt_regs, regs[6]);
	OFFSET(PT_R7, pt_regs, regs[7]);
	OFFSET(PT_R8, pt_regs, regs[8]);
	OFFSET(PT_R9, pt_regs, regs[9]);
	OFFSET(PT_R10, pt_regs, regs[10]);
	OFFSET(PT_R11, pt_regs, regs[11]);
	OFFSET(PT_R12, pt_regs, regs[12]);
	OFFSET(PT_R13, pt_regs, regs[13]);
	OFFSET(PT_R14, pt_regs, regs[14]);
	OFFSET(PT_R15, pt_regs, regs[15]);
	OFFSET(PT_R16, pt_regs, regs[16]);
	OFFSET(PT_R17, pt_regs, regs[17]);
	OFFSET(PT_R18, pt_regs, regs[18]);
	OFFSET(PT_R19, pt_regs, regs[19]);
	OFFSET(PT_R20, pt_regs, regs[20]);
	OFFSET(PT_R21, pt_regs, regs[21]);
	OFFSET(PT_R22, pt_regs, regs[22]);
	OFFSET(PT_R23, pt_regs, regs[23]);
	OFFSET(PT_R24, pt_regs, regs[24]);
	OFFSET(PT_R25, pt_regs, regs[25]);
	OFFSET(PT_R26, pt_regs, regs[26]);
	OFFSET(PT_R27, pt_regs, regs[27]);
	OFFSET(PT_R28, pt_regs, regs[28]);
	OFFSET(PT_R29, pt_regs, regs[29]);
	OFFSET(PT_R30, pt_regs, regs[30]);
	OFFSET(PT_R31, pt_regs, regs[31]);
	OFFSET(PT_STATUS, pt_regs, cp0_status);
	OFFSET(PT_HI, pt_regs, hi);
	OFFSET(PT_LO, pt_regs, lo);
	OFFSET(PT_BVADDR, pt_regs, cp0_badvaddr);
	OFFSET(PT_CAUSE, pt_regs, cp0_cause);
	OFFSET(PT_EPC, pt_regs, cp0_epc);
	DEFINE(PT_SIZE, sizeof(struct pt_regs));

	BLANK();
}

void output_task_defines(void)
{
	COMMENT("TSARMIPS task_struct offsets.");
	/* empty */
	BLANK();
}

void output_thread_info_defines(void)
{
	COMMENT("TSARMIPS thread_info offsets.");
	OFFSET(TI_TASK, thread_info, task);
	OFFSET(TI_FLAGS, thread_info, flags);
	OFFSET(TI_TP_VALUE, thread_info, tp_value);
	OFFSET(TI_ADDR_LIMIT, thread_info, addr_limit);
	OFFSET(TI_KSP, thread_info, ksp);
	BLANK();
}

void output_misc_defines(void)
{
	DEFINE(__PAGE_KERNEL, __PAGE_KERNEL);
	BLANK();
	COMMENT("TSARMIPS pgtable defines.");
	DEFINE(PGD_T_LOG2, PGD_T_LOG2);
	DEFINE(PTRS_PER_PGD_LOG2, PTRS_PER_PGD_LOG2);
	DEFINE(PGDIR_SHIFT, PGDIR_SHIFT);
	DEFINE(PGDIR_SIZE, PGDIR_SIZE);
	BLANK();
}

