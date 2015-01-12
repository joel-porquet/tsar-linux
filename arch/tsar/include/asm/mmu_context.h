#ifndef _ASM_TSAR_MMU_CONTEXT_H
#define _ASM_TSAR_MMU_CONTEXT_H

#include <asm/mmuc2.h>
#include <asm/pgtable.h>

#include <asm-generic/mm_hooks.h>

static inline void enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk)
{
	/* I don't think we need that at the moment */
}

/*
 * in case we define the arch-specific mm->context variable
 * mm->context type is defined in asm/mmu.h
 */
#define init_new_context(tsk, mm)	0

#define destroy_context(mm)		do { } while (0)

static inline void cpu_switch_mm(pgd_t *pgd)
{
	pr_debug("switch_mm: vaddr=%#08lx, paddr=%#010llx\n",
			(unsigned long)pgd, __pa(pgd));
	write_c2_ptpr(__pa(pgd) >> PTPR_SHIFT);
}

static inline void switch_mm(struct mm_struct *prev,
			struct mm_struct *next,
			struct task_struct *tsk)
{
	unsigned int cpu = smp_processor_id();

	/*
	 * update the hardware page table pointer if:
	 * - the new context is not the same as the previous context (eg
	 *   different thread in same @ space)
	 * - or if the new context was not currently running on this cpu
	 */
	if (!cpumask_test_and_set_cpu(cpu, mm_cpumask(next)) || prev != next)
		cpu_switch_mm(next->pgd);

	/* XXX: should we clear 'prev' on the current cpu, as x86 does, to stop
	 * sending flush IPIs? Do we even have flush IPIs? */
}

#define activate_mm(prev,next)		switch_mm(prev, next, NULL)
#define deactivate_mm(tsk,mm)		do { } while (0)

#endif /* _ASM_TSAR_MMU_CONTEXT_H */
