/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/cache.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/topology.h>

#include <asm/mmu_context.h>
#include <asm/numa.h>
#include <asm/smp_map.h>

/*
 * IPI management
 */

/*
 * We can send a few types of message to cpus:
 * - IPI_RESCHEDULE: ask them to reschedule
 * - IPI_CALL_FUNC: ask them to execute a function
 * - IPI_CPU_STOP: ask them to stop
 *
 * We do not need IPI_TIMER because all cpus already have a private timer.
 *
 * We need a IPI_NOP of value 0 though: if not (and IPI_RESCHEDULE == 0), then
 * it causes an infinite loop in handle_IPI (or we should explicitely write
 * msg++ after processing the rescheduling).
 */

enum ipi_msg_type {
	IPI_NOP = 0,
	IPI_RESCHEDULE = 1,
	IPI_CALL_FUNC,
	IPI_CPU_STOP,
};

struct ipi_data {
	unsigned long bits;
};

static DEFINE_PER_CPU(struct ipi_data, ipi_data);

static smp_ipi_call_t *smp_ipi_call;

void __init set_smp_ipi_call(smp_ipi_call_t *new)
{
	smp_ipi_call = new;
}

static void ipi_send_msg(const struct cpumask *mask, enum ipi_msg_type msg)
{
	unsigned long flags;
	int cpu;
	struct ipi_data *ipi;

	pr_debug(CPU_FMT_STR ": sending IPI #%d\n",
			CPU_FMT_ARG(cpu_logical_map(smp_processor_id())),
			(int)msg);

	local_irq_save(flags);

	/* add the message to send to the list */
	for_each_cpu(cpu, mask) {
		ipi = &per_cpu(ipi_data, cpu);
		set_bit(msg, &ipi->bits);
	}

	/* send the ipi signal */
	smp_ipi_call(mask, 0);

	local_irq_restore(flags);
}

void smp_send_reschedule(int cpu)
{
	pr_debug(CPU_FMT_STR ": sending IPI_RESCHEDULE to " CPU_FMT_STR "\n",
			CPU_FMT_ARG(cpu_logical_map(smp_processor_id())),
			CPU_FMT_ARG(cpu_logical_map(cpu)));
	ipi_send_msg(cpumask_of(cpu), IPI_RESCHEDULE);
}

void smp_send_stop(void)
{
	struct cpumask targets;
	cpumask_copy(&targets, cpu_online_mask);
	/* do not include us in the mask */
	cpumask_clear_cpu(smp_processor_id(), &targets);
	pr_debug(CPU_FMT_STR ": sending IPI_CPU_STOP\n",
			CPU_FMT_ARG(cpu_logical_map(smp_processor_id())));
	ipi_send_msg(&targets, IPI_CPU_STOP);
}

void arch_send_call_function_single_ipi(int cpu)
{
	pr_debug(CPU_FMT_STR ": sending IPI_CALL_FUNC to " CPU_FMT_STR "\n",
			CPU_FMT_ARG(cpu_logical_map(smp_processor_id())),
			CPU_FMT_ARG(cpu_logical_map(cpu)));
	ipi_send_msg(cpumask_of(cpu), IPI_CALL_FUNC);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
	pr_debug(CPU_FMT_STR ": sending IPI_CALL_FUNC to some CPUs\n",
			CPU_FMT_ARG(cpu_logical_map(smp_processor_id())));
	ipi_send_msg(mask, IPI_CALL_FUNC);
}

static void ipi_cpu_stop(unsigned int cpu)
{
	pr_crit("CPU%u: stopping\n", cpu);

	local_irq_disable();

	set_cpu_online(cpu, false);

	while (1)
		cpu_relax();
}

void handle_IPI(void)
{
	unsigned int cpu = smp_processor_id();
	struct ipi_data *ipi = &per_cpu(ipi_data, cpu);
	unsigned long ops;

	while ((ops = xchg(&ipi->bits, 0)) != 0) {
		unsigned long msg = 0;

		do {
			msg = find_next_bit(&ops, BITS_PER_LONG, msg + 1);

			switch (msg) {
				case IPI_RESCHEDULE:
					pr_debug(CPU_FMT_STR ": received IPI_RESCHEDULE\n",
							CPU_FMT_ARG(cpu_logical_map(cpu)));
					scheduler_ipi();
					break;
				case IPI_CALL_FUNC:
					pr_debug(CPU_FMT_STR ": received IPI_CALL_FUNC\n",
							CPU_FMT_ARG(cpu_logical_map(cpu)));
					generic_smp_call_function_interrupt();
					break;
				case IPI_CPU_STOP:
					pr_debug(CPU_FMT_STR ": received IPI_CPU_STOP\n",
							CPU_FMT_ARG(cpu_logical_map(cpu)));
					ipi_cpu_stop(cpu);
					break;
				default:
					if (msg < BITS_PER_LONG)
						pr_crit("Unknown IPI on CPU%u: %lu\n",
								cpu, msg);
					break;
			}
		} while (msg < BITS_PER_LONG);
	}
}


/*
 * SMP management - secondary CPUs part
 */

/*
 * C boot code for the secondary CPUs (coming directly from kernel/head.S). At
 * that point, a non-boot cpu is using its idle thread stack and current is set
 * thanks to gp ($28) but the cpu is using a temporary page table (idmap).
 */

asmlinkage void __init secondary_start_kernel(void)
{
	struct mm_struct *mm = &init_mm;
	unsigned int cpu = smp_processor_id();

	/*
	 * Switch away from the idmap page table and use the regular
	 * swapper_pg_dir instead. Update proper mm_context.
	 */
	activate_mm(NULL, mm);
	atomic_inc(&mm->mm_count);
	current->active_mm = mm;
	cpumask_set_cpu(cpu, mm_cpumask(mm));

	/* cpu configuration (e.g. setup exception vector address) */
	cpu_init();

	pr_info(CPU_FMT_STR ": booting secondary processor (logical CPU%u)\n",
			CPU_FMT_ARG(cpu_logical_map(cpu)), cpu);

	preempt_disable();
	trace_hardirqs_off();

	/*
	 * Execute the notifier_block functions that were set by the boot cpu
	 * for the secondary cpus. Basically it means unmasking the hardware
	 * IRQ sources of the cpu, enabling its IPI IRQ and initializing the
	 * private timer.
	 */
	notify_cpu_starting(cpu);

	/* now that that timer is up and running, calibrate the
	 * cpu_loops_per_jiffy for that cpu */
	calibrate_delay();

	/* now the cpu can be considered as being online. this unlocks the boot
	 * cpu who is still waiting for us */
	set_cpu_online(cpu, true);

	/* enable IRQs and start the idle thread */
	local_irq_enable();
	cpu_startup_entry(CPUHP_ONLINE);
}


/*
 * SMP management - boot cpu part
 * The functions are defined in the order they are called.
 * start_kernel
 * 	setup_arch
 * 		smp_init_cpus*
 * 	smp_prepare_boot_cpu*
 * kernel_init
 * 	kernel_init_freeable
 * 		smp_prepare_cpus*
 * 		smp_init
 * 			cpu_up
 * 				_cpu_up
 * 					__cpu_up*
 * 			smp_cpus_done*
 */

/*
 * This function enumerates the possible cpus from the device tree and
 * initializes the cpu logical map accordingly. It also initializes the cpu
 * possible map.
 * Ideally, we could reuse the smp_setup_processor_id() prototype but
 * unfortunately it is called before setup_arch() when the device tree is not
 * yet unflattened...
 */

void __init smp_init_cpus(void)
{
	struct device_node *of_node = NULL;
	unsigned long cpu, i;
	bool bootcpu_valid = false;

	/* now we start with logical cpu #1 */
	cpu = 1;

	while ((of_node = of_find_node_by_type(of_node, "cpu"))) {
		u32 cpuid_reg;

		if (of_property_read_u32(of_node, "reg", &cpuid_reg)) {
			pr_err("%s: cannot find reg property\n",
					of_node->full_name);
			goto next;
		}

		if (cpuid_reg & ~EBASE_CPUHWID) {
			pr_err("%s: invalid reg property\n",
					of_node->full_name);
			goto next;
		}

		/* check the cpuid_reg is not a duplicate of an existing entry */
		for (i = 1; (i < cpu) && (i < NR_CPUS); i++) {
			if (cpu_logical_map(i) == cpuid_reg) {
				pr_err("%s: duplicate reg property\n",
						of_node->full_name);
				goto next;
			}
		}

		/* at some point, probably during the first loop, we will
		 * encounter the of node that describes the boot cpu. keep
		 * track of it. */
		if (cpuid_reg == cpu_logical_map(0)) {
			if (bootcpu_valid) {
				pr_err("%s: duplicate bootcpu reg property\n",
						of_node->full_name);
				goto next;
			}
			bootcpu_valid = true;
			/* no need to increment cpu in that case, the bootcpu
			 * already uses the index #0 */
			continue;
		}

		/* some more error checking */
		if (cpu >= NR_CPUS)
			goto next;

		/* now we can register the discovered cpu node in the logical
		 * map */
		pr_debug("logical cpu %ld = physical cpu 0x%x (" CPU_FMT_STR ")\n",
				cpu, cpuid_reg, CPU_FMT_ARG((long)cpuid_reg));
		cpu_logical_map(cpu) = cpuid_reg;
next:
		cpu++;
	}

	/* sanity checks */
	if (cpu > NR_CPUS)
		pr_warning("The number of cpus (%ld) in the DT is greater than"
				"the configured number (%d)\n", cpu, NR_CPUS);

	if (!bootcpu_valid) {
		pr_err("The DT misses bootcpu node! Discard secondary cpus!\n");
		return;
	}

	/* All the cpus added to the logical map can now be set as possible cpus */
	for (i = 0; i < NR_CPUS; i++)
	{
		if (cpu_logical_map(i) != INVALID_HWCPUID)
			set_cpu_possible(i, true);
	}
}

void __init smp_prepare_boot_cpu(void)
{
	int cpu;

	/* The association between cpus and nodes could only be done for the
	 * boot cpu here, and later by secondary cpus when they boot. But we
	 * need cpu_to_node() to be operational when using SMP_IPI_BOOT, so
	 * let's do the whole association thing here and be done with it. */
	for_each_possible_cpu(cpu) {
		set_cpu_numa_node(cpu, cpu_node_map[cpu]);
	}
}

/*
 * Cpu present map initialization
 */

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	unsigned int ncpus = num_possible_cpus();

	/* max_cpus comes from setup_max_cpus. setup_max_cpus is initialized to
	 * be NR_CPUS but can be modified by arguments (nosmp and maxcpus=n) */
	/* in either cases, we must check we are not trying to boot more cpus
	 * than what we discovered in smp_init_cpus, with the device tree */
	if (max_cpus > ncpus)
		max_cpus = ncpus;

	if (ncpus > 1 && max_cpus)
		/* initialize the cpu present map, merely by copying the cpu
		 * possible map */
		init_cpu_present(cpu_possible_mask);
}

/*
 * Boot the specified secondary cpu
 */

volatile unsigned long secondary_cpu_boot __cacheline_aligned = INVALID_HWCPUID;
volatile unsigned long secondary_cpu_gp __cacheline_aligned;

#ifdef CONFIG_SMP_IPI_BOOT
extern void secondary_kernel_entry(void);
#endif

int __cpu_up(unsigned int cpu, struct task_struct *idle)
{
	int ret = 0;
	unsigned long timeout;

	/* setup the secondary_cpu_gp to tell the cpu where to find its global
	 * pointer (i.e. its idle thread_info structure) */
	secondary_cpu_gp = (unsigned long)task_thread_info(idle);
	/* unlock cpu from spin wait and make it boot */
	secondary_cpu_boot = cpu_logical_map(cpu);

	/* force flushing writes in memory */
	wmb();

#ifdef CONFIG_SMP_IPI_BOOT
	/* in IPI boot, secondary cpus have been put to sleep by the preloader.
	 * They are waiting for an IPI to tell them where to jump to. In this
	 * case, they jump directly to the secondary entry point. */
	smp_ipi_call(cpumask_of(cpu), __pa(secondary_kernel_entry));
#endif

	/* wait 1s until the cpu is online */
	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		if (cpu_online(cpu))
			break;
		udelay(10);
	}

	if (!cpu_online(cpu)) {
		pr_crit("CPU%u: failed to come online\n", cpu);
		ret = -EIO;
	}

	/* reset the secondary data */
	secondary_cpu_boot = INVALID_HWCPUID;
	secondary_cpu_gp = 0;

	return ret;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
	pr_info("SMP: Total of %d processors activated.\n", num_online_cpus());
}


/*
 * not supported
 */

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}
