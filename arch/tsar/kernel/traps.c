/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/kdb.h>
#include <linux/kdebug.h>
#include <linux/kgdb.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include <asm/mips32c0.h>
#include <asm/branch.h>
#include <asm/mmuc2.h>
#include <asm/ptrace.h>
#include <asm/traps.h>

unsigned long exception_handlers[32];

/*
 * Support functions for status printing
 */

static void show_backtrace(unsigned long _sp)
{
	/* make sure the stack pointer is aligned on a 8 bytes boundary */
	unsigned long *sp = (unsigned long *)(_sp & ~3);
	unsigned long addr;

	printk("Call Trace:");
#ifdef CONFIG_KALLSYMS
	printk("\n");
#endif

	while (!kstack_end(sp)) {
		unsigned long __user *p =
			(unsigned long __user *)(unsigned long)sp++;
		if (__get_user(addr, p)) {
			printk(" (Bad stack address)");
			break;
		}
		if (__kernel_text_address(addr)) {
			print_ip_sym(addr);
		}
	}
	printk("\n");
}

static void show_stacktrace(unsigned long _sp)
{
	/* make sure the stack pointer is aligned on a 8 bytes boundary */
	unsigned long *sp = (unsigned long *)(_sp & ~3);
	const int field = 2 * sizeof(unsigned long);
	long stackdata;
	int i;

	printk("Stack:");
	i = 0;
	while ((unsigned long) sp & (PAGE_SIZE - 1)) {
		unsigned long __user *p =
			(unsigned long __user *)(unsigned long)sp++;
		if (i && ((i % (64 / field)) == 0))
			printk("\n       ");
		if (i > 39) {
			printk(" ...");
			break;
		}

		if (__get_user(stackdata, p)) {
			printk(" (Bad stack address)");
			break;
		}

		printk(" %0*lx", field, stackdata);
		i++;
	}
	printk("\n");
	show_backtrace(_sp);
}

void show_stack(struct task_struct *task, unsigned long *sp)
{
	unsigned long _sp = 0;
	const register unsigned long current_sp asm ("sp");

	if (sp) {
		_sp = (unsigned long)sp;
	} else {
		if (task && task != current) {
			_sp = task_pt_regs(task)->regs[29];
#ifdef CONFIG_KGDB_KDB
		} else if (atomic_read(&kgdb_active) != -1
				&& kdb_current_regs) {
			_sp = kdb_current_regs->regs[29];
#endif
		} else {
			_sp = current_sp;
		}
	}
	show_stacktrace(_sp);
}

static void show_code(unsigned long *pc)
{
	long i;

	printk("\nCode:");

	for (i = -3; i < 6; i++) {
		unsigned long insn;
		if (__get_user(insn, pc + i)) {
			printk(" (Bad address in epc)\n");
			break;
		}
		printk("%c%08lx%c", (i ? ' ' : '<'),
				insn, (i ? ' ' : '>'));
	}
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

	printk("PrId  : %08lx\n", read_c0_prid());
}

/* short version */
void show_regs(struct pt_regs *regs)
{
	__show_regs(regs);
}

/* long version */
void show_registers(struct pt_regs *regs)
{
	unsigned long tls;
	const int field = 2 * sizeof(unsigned long);

	show_regs(regs);
	//print_modules();
	printk("Process %s (pid: %d, threadinfo=%p, task=%p, tls=%0*lx)\n",
	       current->comm, current->pid, current_thread_info(), current,
	      field, current_thread_info()->tp_value);

	tls = read_c0_userlocal();
	if (tls != current_thread_info()->tp_value)
		printk("*HwTLS: %0*lx\n", field, tls);

	show_stacktrace(regs->regs[29]);
	show_code((unsigned long __user *) regs->cp0_epc);
	printk("\n");
}

/*
 * Support functions for dying...
 */

static int regs_to_trapnr(struct pt_regs *regs)
{
	return cause_exccode(regs->cp0_cause);
}

static DEFINE_RAW_SPINLOCK(die_lock);

void __noreturn die(const char *str, struct pt_regs *regs)
{
	static int die_counter;
	int sig = SIGSEGV;

	oops_enter();

	if (notify_die(DIE_OOPS, str, regs, 0, regs_to_trapnr(regs),
				SIGSEGV) == NOTIFY_STOP)
		sig = 0;

	console_verbose();
	raw_spin_lock_irq(&die_lock);
	bust_spinlocks(1);

	pr_emerg("\n%s[#%d]:\n", str, ++die_counter);
	show_registers(regs);

	bust_spinlocks(0);
	add_taint(TAINT_DIE, LOCKDEP_NOW_UNRELIABLE);
	raw_spin_unlock_irq(&die_lock);
	oops_exit();

	if (in_interrupt())
		panic("Fatal exception in interrupt");
	if (panic_on_oops)
		panic("Fatal exception");

	do_exit(sig);
}

void die_if_kernel(const char *str, struct pt_regs *regs)
{
	if (!user_mode(regs))
		die(str, regs);
}

/*
 * Breakpoints and traps
 */

static void do_trap_or_bp(struct pt_regs *regs, unsigned long code,
		const char *str)
{
	if (notify_die(DIE_OOPS, str, regs, code, regs_to_trapnr(regs),
				SIGTRAP) == NOTIFY_STOP)
		return;

	die_if_kernel("Trap/Breakpoint instruction in kernel code", regs);
	force_sig(SIGTRAP, current);
}

/*
 * Exception handlers
 */

asmlinkage void do_reserved(struct pt_regs *regs)
{
	unsigned long ex_code;
	ex_code = regs_to_trapnr(regs);
	show_regs(regs);
	panic("do_reserved: ex_code=%ld (this should never happen!)\n",
			ex_code);
}

asmlinkage void do_ade(struct pt_regs *regs)
{
	pr_alert("do_ade: epc=0x%08lx, bvaddr=0x%08lx\n",
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

	pr_alert("Instruction bus error, epc=%08lx, ra=%08lx\n",
			regs->cp0_epc, regs->regs[31]);
	pr_alert("IBE: ptpr=0x%08lx, ietr=0x%08lx, ibvar=0x%08lx\n",
			mmu_ptpr, mmu_ietr, mmu_ibvar);

	if (notify_die(DIE_OOPS, "instruction bus error", regs, 0,
				regs_to_trapnr(regs), SIGBUS) == NOTIFY_STOP)
		return;

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

	pr_alert("Data bus error: epc=0x%08lx, ra=0x%08lx\n",
			regs->cp0_epc, regs->regs[31]);
	pr_alert("DBE: ptpr=0x%08lx, detr=0x%08lx, dbvar=0x%08lx\n",
			mmu_ptpr, mmu_detr, mmu_dbvar);

	if (notify_die(DIE_OOPS, "data bus error", regs, 0,
				regs_to_trapnr(regs), SIGBUS) == NOTIFY_STOP)
		return;

	die_if_kernel("do_dbe in kernel", regs);
	force_sig(SIGBUS, current);
}

asmlinkage void do_bp(struct pt_regs *regs)
{
	unsigned long opcode, bcode;

	/* get the instruction to get the breakpoint code */
	if (__get_user(opcode, (unsigned long __user *)exception_epc(regs))) {
		force_sig(SIGSEGV, current);
		return;
	}

	/* breakpoint instructions include a the code field (25:6) */
	bcode = ((opcode >> 6) & ((1 << 20) - 1));
	do_trap_or_bp(regs, bcode, "breakpoint");
}

asmlinkage void do_ri(struct pt_regs *regs)
{
	unsigned long epc;
	unsigned long bd;
	epc = regs->cp0_epc;
	bd = cause_bd(regs->cp0_cause);
	pr_debug("do_ri: epc=0x%08lx, bd=%ld\n", epc, bd);

	if (notify_die(DIE_OOPS, "reserved instruction", regs, 0,
				regs_to_trapnr(regs), SIGILL) == NOTIFY_STOP)
		return;

	die_if_kernel("do_ri in kernel", regs);
	force_sig(SIGILL, current);
}

asmlinkage void do_cpu(struct pt_regs *regs)
{
	unsigned long cpid;
	cpid = cause_ce(regs->cp0_cause);
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
	unsigned long opcode, tcode;

	if (__get_user(opcode, (unsigned long __user *)exception_epc(regs))) {
		force_sig(SIGSEGV, current);
		return;
	}

	if (!(opcode & 0xfc000000))
		/* trap operations with a code field (15:6) */
		tcode = ((opcode >> 6) & ((1 << 10) - 1));
	else
		/* immediate trap operations without any code field */
		tcode = 0;

	do_trap_or_bp(regs, tcode, "trap");
}

/*
 * Exception/trap initialization
 */

/* This function is called by all the cpus during their boot.
 * It configures them with the right exception vector address and enable
 * certains system registers for user access.
 */
void __init cpu_init(void)
{
	/* set the new EBASE in the cpu */
	write_c0_ebase(general_exception_vector);

	/*
	 * allow the usermode to access certain CP0 registers (cpunum, count,
	 * userlocal/tls, etc.) using the rdhwr instruction
	 */
	write_c0_hwrena(HWRENAF_ULR | HWRENAF_CCRES | HWRENAF_CC |
			HWRENAF_CPUNUM);
}

static void __init set_except_vector(int n, void *handler)
{
	exception_handlers[n] = (unsigned long)handler;
}

/*
 * This function is only called once, by the boot cpu.
 * It installs the different exception handlers, and call the trap
 * initialization for the boot cpu.
 */
void __init trap_init(void)
{
	unsigned int i;

	/* install exception handlers */
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

	/* configure the boot cpu */
	cpu_init();
}
