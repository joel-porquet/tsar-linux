/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

/*
 * IPI management
 */

/*
 * We can send a few types of message to cpus:
 * - IPI_RESCHEDULE: ask them to reschedule
 * - IPI_CALL_FUNC: ask them to execute a function
 * - IPI_CPU_STOP: ask them to stop
 * We do not need IPI_TIMER because all cpus already have a private timer.
 */
enum ipi_msg_type {
	IPI_RESCHEDULE,
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

	local_irq_save(flags);

	/* add the message to send to the list */
	for_each_cpu(cpu, mask) {
		ipi = &per_cpu(ipi_data, cpu);
		set_bit(msg, &ipi->bits);
	}

	/* send the ipi signal */
	smp_ipi_call(mask);

	local_irq_restore(flags);
}

void smp_send_reschedule(int cpu)
{
	ipi_send_msg(cpumask_of(cpu), IPI_RESCHEDULE);
}

void smp_send_stop(void)
{
	struct cpumask targets;
	cpumask_copy(&targets, cpu_online_mask);
	/* do not include us in the mask */
	cpumask_clear_cpu(smp_processor_id(), &targets);
	ipi_send_msg(&targets, IPI_CPU_STOP);
}

void arch_send_call_function_single_ipi(int cpu)
{
	ipi_send_msg(cpumask_of(cpu), IPI_CALL_FUNC);
}

void arch_send_call_function_ipi_mask(const struct cpumask *mask)
{
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
		unsigned long msg;

		do {
			msg = find_next_bit(ops, BITS_PER_LONG, msg + 1);

			switch(msg) {
				case IPI_RESCHEDULE:
					scheduler_ipi();
					break;
				case IPI_CALL_FUNC:
					generic_smp_call_function_interrupt();
					break;
				case IPI_CPU_STOP:
					ipi_cpu_stop(cpu);
					break;
				default:
					pr_crit("Unknown IPI on CPU%u: %lu\n",
							cpu, msg);
					break;
			}
		} while (msg < BITS_PER_LONG);
	}
}

/*
 * SMP management
 */
