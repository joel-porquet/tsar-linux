#ifndef _ASM_TSAR_PROCESSOR_H
#define _ASM_TSAR_PROCESSOR_H

#include <linux/cpumask.h>
#include <linux/threads.h>

#include <asm/page.h>
#include <asm/ptrace.h>

/*
 * Return current instruction pointer ("program counter")
 */
#define current_text_addr() ({ __label__ _l; _l: &&_l;})

/*
 * User space process size (in our arch, this size can reach as high as
 * PAGE_OFFSET which is where the kernel starts)
 *
 * Stack starts at the very top of the user space process address space
 */
#define TASK_SIZE		PAGE_OFFSET
#define STACK_TOP		TASK_SIZE
#define STACK_TOP_MAX		STACK_TOP

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's (also used when loading dynamic libraries)
 */
#define TASK_UNMAPPED_BASE PAGE_ALIGN(TASK_SIZE / 3)

#ifndef __ASSEMBLY__

typedef struct {
	unsigned long seg;
} mm_segment_t;

/* As Openrisc, and unlike MIPS, we don't use thread_struct to save registers
 * but we stack a pt_regs on the kernel stack */
struct thread_struct { };

#define INIT_THREAD { }

struct task_struct;

/* Free all resources held by a thread. */
#define release_thread(thread) do { } while(0)

extern unsigned long thread_saved_pc(struct task_struct *t);

/*
 * Do necessary setup to start up a newly executed thread.
 */
extern void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp);

unsigned long get_wchan(struct task_struct *p);

/* For every task (user and kernel), there is a pt_regs struct stacked on top
 * of the kernel stack. */
#define task_pt_regs(tsk) ((struct pt_regs *)((unsigned long)task_thread_info(tsk) \
			+ THREAD_SIZE - sizeof(struct pt_regs)))

#define KSTK_EIP(tsk) (task_pt_regs(tsk)->cp0_epc)
#define KSTK_ESP(tsk) (task_pt_regs(tsk)->regs[29])
#define KSTK_STATUS(tsk) (task_pt_regs(tsk)->cp0_status)

#define cpu_relax()	barrier()

/*
 * Return_address is a replacement for __builtin_return_address(count)
 * which on certain architectures cannot reasonably be implemented in GCC
 * (MIPS, Alpha) or is unusable with -fomit-frame-pointer (i386).
 * Note that __builtin_return_address(x>=1) is forbidden because GCC
 * aborts compilation on some CPUs.  It's simply not possible to unwind
 * some CPU's stackframes.
 *
 * __builtin_return_address works only for non-leaf functions.	We avoid the
 * overhead of a function call by forcing the compiler to save the return
 * address register on the stack.
 */
#define return_address() ({__asm__ __volatile__("":::"$31");__builtin_return_address(0);})

/* proc/cpuinfo */
extern const struct seq_operations cpuinfo_op;

#endif /* __ASSEMBLY__ */
#endif /* _ASM_TSAR_PROCESSOR_H */
