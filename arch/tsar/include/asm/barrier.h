#ifndef _ASM_TSAR_BARRIER_H
#define _ASM_TSAR_BARRIER_H

#define read_barrier_depends()		do {} while (0)
#define smp_read_barrier_depends()	do {} while (0)

/* the "sync" in mips forces to empty the write buffer */
#define __sync() __asm__ __volatile__("sync" ::: "memory")

/* general memory barrier: complete all memory accesses, read and write
 * operations, before this point */
#define mb()	__sync()

/* read memory barrier: complete all read accesses before this point.
 * in our case, read operations are blocking operations without reordering, so
 * we just need a compiler barrier */
#define rmb()	barrier()

/* write memory barrier: complete all write accesses before this point. */
#define wmb()	__sync()

#ifdef CONFIG_SMP
/* SMP configuration */
#define smp_mb()	mb()
#define smp_rmb()	rmb()
#define smp_wmb()	wmb()
#else
/* when UP, just compiler barriers */
#define smp_mb()	barrier()
#define smp_rmb()	barrier()
#define smp_wmb()	barrier()
#endif

/* ll/sc do not trigger an automatic hardware barrier, so force it */
#define smp_mb__before_llsc()	smp_wmb()
/* sc is an uncached access, no need to force hardware commit to memory */
#define smp_mb__after_llsc()	barrier()

#define set_mb(var, value)  do { (var) = (value); mb(); } while (0)

#endif /* _ASM_TSAR_BARRIER_H */
