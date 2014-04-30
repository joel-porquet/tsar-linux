/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/kdebug.h>
#include <linux/kgdb.h>
#include <linux/ptrace.h>

/*
 * KDB interface
 */

struct dbg_reg_def_t dbg_reg_def[DBG_MAX_REG_NUM] = {
	{ "zero", 4, offsetof(struct pt_regs, regs[0]) },
	{ "at", 4, offsetof(struct pt_regs, regs[1]) },
	{ "v0", 4, offsetof(struct pt_regs, regs[2]) },
	{ "v1", 4, offsetof(struct pt_regs, regs[3]) },
	{ "a0", 4, offsetof(struct pt_regs, regs[4]) },
	{ "a1", 4, offsetof(struct pt_regs, regs[5]) },
	{ "a2", 4, offsetof(struct pt_regs, regs[6]) },
	{ "a3", 4, offsetof(struct pt_regs, regs[7]) },
	{ "t0", 4, offsetof(struct pt_regs, regs[8]) },
	{ "t1", 4, offsetof(struct pt_regs, regs[9]) },
	{ "t2", 4, offsetof(struct pt_regs, regs[10]) },
	{ "t3", 4, offsetof(struct pt_regs, regs[11]) },
	{ "t4", 4, offsetof(struct pt_regs, regs[12]) },
	{ "t5", 4, offsetof(struct pt_regs, regs[13]) },
	{ "t6", 4, offsetof(struct pt_regs, regs[14]) },
	{ "t7", 4, offsetof(struct pt_regs, regs[15]) },
	{ "s0", 4, offsetof(struct pt_regs, regs[16]) },
	{ "s1", 4, offsetof(struct pt_regs, regs[17]) },
	{ "s2", 4, offsetof(struct pt_regs, regs[18]) },
	{ "s3", 4, offsetof(struct pt_regs, regs[19]) },
	{ "s4", 4, offsetof(struct pt_regs, regs[20]) },
	{ "s5", 4, offsetof(struct pt_regs, regs[21]) },
	{ "s6", 4, offsetof(struct pt_regs, regs[22]) },
	{ "s7", 4, offsetof(struct pt_regs, regs[23]) },
	{ "t8", 4, offsetof(struct pt_regs, regs[24]) },
	{ "t9", 4, offsetof(struct pt_regs, regs[25]) },
	{ "k0", 4, offsetof(struct pt_regs, regs[26]) },
	{ "k1", 4, offsetof(struct pt_regs, regs[27]) },
	{ "gp", 4, offsetof(struct pt_regs, regs[28]) },
	{ "sp", 4, offsetof(struct pt_regs, regs[29]) },
	{ "s8", 4, offsetof(struct pt_regs, regs[30]) },
	{ "ra", 4, offsetof(struct pt_regs, regs[31]) },
	{ "sr", 4, offsetof(struct pt_regs, cp0_status) },
	{ "lo", 4, offsetof(struct pt_regs, lo) },
	{ "hi", 4, offsetof(struct pt_regs, hi) },
	{ "bad", 4, offsetof(struct pt_regs, cp0_badvaddr) },
	{ "cause", 4, offsetof(struct pt_regs, cp0_cause) },
	{ "pc", 4, offsetof(struct pt_regs, cp0_epc) },
};

char *dbg_get_reg(int regno, void *mem, struct pt_regs *regs)
{
	if (regno >= DBG_MAX_REG_NUM || regno < 0)
		return NULL;

	if (dbg_reg_def[regno].offset != -1)
		memcpy(mem, (void *)regs + dbg_reg_def[regno].offset,
				dbg_reg_def[regno].size);
	else
		memset(mem, 0, dbg_reg_def[regno].size);
	return dbg_reg_def[regno].name;
}

int dbg_set_reg(int regno, void *mem, struct pt_regs *regs)
{
	if (regno >= DBG_MAX_REG_NUM || regno < 0)
		return -EINVAL;

	if (dbg_reg_def[regno].offset != -1)
		memcpy((void *)regs + dbg_reg_def[regno].offset, mem,
				dbg_reg_def[regno].size);
	return 0;
}

/*
 * KGDB interface
 */

void sleeping_thread_to_gdb_regs(unsigned long *gdb_regs, struct task_struct *p)
{
	int reg;
	struct pt_regs *regs;

	if (p == NULL)
		return;

	regs = task_pt_regs(p);

	for (reg = 0; reg < DBG_MAX_REG_NUM; reg++)
		dbg_get_reg(reg, gdb_regs + reg, regs);
}

#ifdef CONFIG_SMP
/*
 * make other cpus call kgdb_wait(), using IPIs
 */

static void kgdb_call_nmi_hook(void *ignored)
{
	kgdb_nmicallback(raw_smp_processor_id(), NULL);
}

void kgdb_roundup_cpus(unsigned long flags)
{
	local_irq_enable();
	smp_call_function(kgdb_call_nmi_hook, NULL, 0);
	local_irq_disable();
}
#endif

void kgdb_arch_set_pc(struct pt_regs *regs, unsigned long pc)
{
	instruction_pointer_set(regs, pc);
}

int kgdb_arch_handle_exception(int vector, int signo, int err_code,
		char *remcom_in_buffer,
		char *remcom_out_buffer,
		struct pt_regs *regs)
{
	char *ptr;
	unsigned long address;

	switch (remcom_in_buffer[0]) {
		case 'c':
			ptr = &remcom_in_buffer[1];
			if (kgdb_hex2long(&ptr, &address))
				instruction_pointer_set(regs, address);
			return 0;
	}

	return -1;
}

static int __kgdb_notify(struct die_args *args, unsigned long cmd)
{
	/* cpu roundup */
	if (atomic_read(&kgdb_active) != -1) {
		kgdb_nmicallback(smp_processor_id(), args->regs);
		return NOTIFY_STOP;
	}

	if (user_mode(args->regs))
		return NOTIFY_DONE;

	if (kgdb_handle_exception(args->trapnr, args->signr, args->err,
				args->regs))
		return NOTIFY_DONE;

	/* jump over the trap instruction */
	if (*(unsigned long *)instruction_pointer(args->regs) ==
			*(unsigned long *)(&arch_kgdb_ops.gdb_bpt_instr))
		instruction_pointer_set(args->regs,
				instruction_pointer(args->regs) + BREAK_INSTR_SIZE);

	return NOTIFY_STOP;
}

static int kgdb_notify(struct notifier_block *self, unsigned long cmd, void
		*ptr)
{
	unsigned long flags;
	int ret;

	local_irq_save(flags);
	ret = __kgdb_notify(ptr, cmd);
	local_irq_restore(flags);

	return ret;
}

static struct notifier_block kgdb_notifier = {
	.notifier_call = kgdb_notify,
	/* lowest priority */
	.priority = -INT_MAX,
};

int kgdb_arch_init(void)
{
	/* get kgdb when kernel dies (e.g. panic) */
	register_die_notifier(&kgdb_notifier);
	return 0;
}

void kgdb_arch_exit(void)
{
	unregister_die_notifier(&kgdb_notifier);
}

struct kgdb_arch arch_kgdb_ops = {
	/* 31-26: special opcode (i.e. 000000)
	 * 25-6: break code
	 * 5-0: 001101
	 */
	.gdb_bpt_instr = {0x0d, 0x00, 0x00, 0x00},
};

