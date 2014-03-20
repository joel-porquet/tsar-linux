/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/sched.h>

#include <asm/mips32c0.h>
#include <asm/mmuc2.h>
#include <asm/ptrace.h>
#include <asm/traps.h>

unsigned long exception_handlers[32];

/*
 * Support functions for status printing
 */

void show_stack(struct task_struct *task, unsigned long *sp)
{
	printk("Show stack!\n");
}

static void __show_regs(const struct pt_regs *regs)
{
	const int field = 2 * sizeof(unsigned long);
	unsigned long cause = regs->cp0_cause;
	int i;

	show_regs_print_info(KERN_DEFAULT);

	/*
	 * Saved main processor registers
	 */
	for (i = 0; i < 32; ) {
		if ((i % 4) == 0)
			printk("$%2d   :", i);
		if (i == 0)
			printk(" %0*lx", field, 0UL);
		else if (i == 26 || i == 27)
			printk(" %*s", field, "");
		else
			printk(" %0*lx", field, regs->regs[i]);

		i++;
		if ((i % 4) == 0)
			printk("\n");
	}

	printk("Hi    : %0*lx\n", field, regs->hi);
	printk("Lo    : %0*lx\n", field, regs->lo);

	/*
	 * Saved cp0 registers
	 */
	printk("epc   : %0*lx %pS\n", field, regs->cp0_epc,
			(void *) regs->cp0_epc);
	printk("    %s\n", print_tainted());
	printk("ra    : %0*lx %pS\n", field, regs->regs[31],
			(void *) regs->regs[31]);

	printk("Status: %08lx	", regs->cp0_status);

	switch (regs->cp0_status & ST0_KSU) {
		case ST0_KSU_USER:
			printk("USER ");
			break;
		case ST0_KSU_SUPERVISOR:
			printk("SUPERVISOR ");
			break;
		case ST0_KSU_KERNEL:
			printk("KERNEL ");
			break;
		default:
			printk("BAD_MODE ");
			break;
	}
	if (regs->cp0_status & ST0_ERL)
		printk("ERL ");
	if (regs->cp0_status & ST0_EXL)
		printk("EXL ");
	if (regs->cp0_status & ST0_IE)
		printk("IE ");
	printk("\n");

	printk("Cause : %08lx\n", cause);

	cause = (cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE;

	printk("PrId  : %08lx\n", read_c0_prid());
}

void show_regs(struct pt_regs *regs)
{
	__show_regs((struct pt_regs *)regs);
}

/*
 * Support functions for dying...
 */

void die(const char *str, struct pt_regs *regs)
{
	console_verbose();
	pr_emerg("\n%s\n", str);
	show_regs(regs);
	do_exit(SIGSEGV);
}

void die_if_kernel(const char *str, struct pt_regs *regs)
{
	if (user_mode(regs))
		return;
	die(str, regs);
}

/*
 * Exception handlers
 */

asmlinkage void do_reserved(struct pt_regs *regs)
{
	unsigned long ex_code;
	ex_code = (regs->cp0_cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE;
	pr_debug("do_reserved: ex_code=%ld (this should never happen!)\n",
			ex_code);
	/* let's really die, since this event should never happen */
	die("do_reserved exception", regs);
}

asmlinkage void do_ade(struct pt_regs *regs)
{
	printk("do_ade: epc=0x%08lx, bvaddr=0x%08lx\n",
			regs->cp0_epc, regs->cp0_badvaddr);
	die_if_kernel("do_ade in kernel", regs);
	force_sig(SIGBUS, current);
}

asmlinkage void do_ibe(struct pt_regs *regs)
{
	unsigned long mmu_ptpr = read_c2_ptpr() << 13;
	unsigned long mmu_ietr = read_c2_ietr();
	unsigned long mmu_ibvar = read_c2_ibvar();

	pr_debug("do_ibe: epc=0x%08lx\n", regs->cp0_epc);

	pr_debug("IBE: ptpr=0x%08lx, ietr=0x%08lx, ibvar=0x%08lx\n",
			mmu_ptpr, mmu_ietr, mmu_ibvar);

	/* a IBE is necessarily a read access */
	BUG_ON((mmu_ietr & MMU_RW_MASK) != MMU_ETR_READ);

	switch (mmu_ietr & MMU_ERR_MASK)
	{
		case MMU_PT1_UNMAPPED:
		case MMU_PT2_UNMAPPED:
		case MMU_EXEC_VIOLATION:
			do_page_fault(regs, mmu_ibvar, 1, 0);
			return;
		case MMU_PT1_ILLEGAL_ACCESS:
		case MMU_PT2_ILLEGAL_ACCESS:
		case MMU_DATA_ILLEGAL_ACCESS:
			die("the page table seems to be erroneous", regs);
			break;
		case MMU_UNDEFINED_XTN:
			pr_err("undefined XTN command\n");
			break;
		case MMU_PRIVILEGE_VIOLATION:
			BUG_ON(!user_mode(regs));
			pr_err("privilege violation\n");
			break;
	}

	die_if_kernel("do_ibe in kernel", regs);
	force_sig(SIGBUS, current);
}

asmlinkage void do_dbe(struct pt_regs *regs)
{
	unsigned long mmu_ptpr = read_c2_ptpr() << 13;
	unsigned long mmu_detr = read_c2_detr();
	unsigned long mmu_dbvar = read_c2_dbvar();
	unsigned long mmu_write_acc;

	pr_debug("do_dbe: epc=0x%08lx\n", regs->cp0_epc);

	pr_debug("DBE: ptpr=0x%08lx, detr=0x%08lx, dbvar=0x%08lx\n",
			mmu_ptpr, mmu_detr, mmu_dbvar);

	mmu_write_acc = ((mmu_detr & MMU_RW_MASK) == MMU_ETR_WRITE);

	switch (mmu_detr & MMU_ERR_MASK)
	{
		case MMU_PT1_UNMAPPED:
		case MMU_PT2_UNMAPPED:
		case MMU_ACCES_VIOLATION:
			do_page_fault(regs, mmu_dbvar, 0, mmu_write_acc);
			return;
		case MMU_PT1_ILLEGAL_ACCESS:
		case MMU_PT2_ILLEGAL_ACCESS:
		case MMU_DATA_ILLEGAL_ACCESS:
			die("the page table seems to be erroneous", regs);
			break;
		case MMU_UNDEFINED_XTN:
			pr_err("undefined XTN command\n");
			break;
		case MMU_PRIVILEGE_VIOLATION:
			BUG_ON(!user_mode(regs));
			pr_err("privilege violation\n");
			break;
	}

	die_if_kernel("do_dbe in kernel", regs);
	force_sig(SIGBUS, current);
}

asmlinkage void do_bp(struct pt_regs *regs)
{
	die_if_kernel("do_bp in kernel", regs);
	force_sig(SIGTRAP, current);
}

asmlinkage void do_ri(struct pt_regs *regs)
{
	unsigned long epc;
	unsigned long bd;
	epc = regs->cp0_epc;
	bd = (regs->cp0_cause & CAUSEF_BD) >> CAUSEB_BD;
	pr_debug("do_ri: epc=0x%08lx, bd=%ld\n", epc, bd);

	die_if_kernel("do_ri in kernel", regs);
	force_sig(SIGILL, current);
}

asmlinkage void do_cpu(struct pt_regs *regs)
{
	unsigned long cpid;
	cpid = (regs->cp0_cause & CAUSEF_CE) >> CAUSEB_CE;
	pr_debug("do_cpu: cpid=%ld\n", cpid);

	die_if_kernel("do_cpu in kernel", regs);
	force_sig(SIGILL, current);
}

asmlinkage void do_ov(struct pt_regs *regs)
{
	siginfo_t info;

	die_if_kernel("do_ov in kernel", regs);

	info.si_code = FPE_INTOVF;
	info.si_signo = SIGFPE;
	info.si_errno = 0;
	info.si_addr = (void *) regs->cp0_epc;
	force_sig_info(SIGFPE, &info, current);
}

asmlinkage void do_tr(struct pt_regs *regs)
{
	die_if_kernel("do_tr in kernel", regs);
	force_sig(SIGTRAP, current);
}

/*
 * Exception/trap initialization
 */

static void set_except_vector(int n, void *handler)
{
	exception_handlers[n] = (unsigned long)handler;
}

void __init trap_init(void)
{
	unsigned int i;
	void __iomem *ebase_virt;

	/* allocate enough space for the general exception vector:
	 * - it must be located at offset 0x180 of a memory area aligned on
	 *   0x1000
	 * - there's only two instructions in this vector (a jump and its
	 *   associated delay slot) */
	ebase_virt = __va(memblock_alloc(0x180 + 8, 0x1000));
	pr_debug("trap_init: allocate general exception vector at @0x%p\n",
			ebase_virt);

	/* copy the general exception vector into this new area */
	memcpy(ebase_virt + 0x180, &general_exception_vector, 8);

	/* set the new EBASE in the cpu */
	write_c0_ebase(ebase_virt);

	/* initialize exception handlers */
	for (i = 0; i < 32; i++)
		set_except_vector(i, handle_reserved);

	set_except_vector(CAUSE_EXCCODE_INT, handle_int);

	set_except_vector(CAUSE_EXCCODE_ADEL, handle_ade);
	set_except_vector(CAUSE_EXCCODE_ADES, handle_ade);

	set_except_vector(CAUSE_EXCCODE_IBE, handle_ibe);
	set_except_vector(CAUSE_EXCCODE_DBE, handle_dbe);

	set_except_vector(CAUSE_EXCCODE_SYS, handle_sys);

	set_except_vector(CAUSE_EXCCODE_BP, handle_bp);
	set_except_vector(CAUSE_EXCCODE_RI, handle_ri);
	set_except_vector(CAUSE_EXCCODE_CPU, handle_cpu);
	set_except_vector(CAUSE_EXCCODE_OV, handle_ov);
	set_except_vector(CAUSE_EXCCODE_TR, handle_tr);

	//set_except_vector(CAUSE_EXCCODE_FPE, handle_fpe);

	/*
	 * allow the usermode to access certain CP0 registers (cpunum, count,
	 * userlocal/tls, etc.) using the rdhwr instruction
	 */
	write_c0_hwrena(HWRENAF_ULR | HWRENAF_CCRES | HWRENAF_CC |
			HWRENAF_CPUNUM);
}
