#ifndef _ASM_TSAR_ATOMIC_H
#define _ASM_TSAR_ATOMIC_H

#include <linux/types.h>

#include <asm/barrier.h>

#ifdef CONFIG_SMP

/*
 * Add or substract integer to atomic variable
 */

#define ATOMIC_RETURN(fn, op)                                       \
static inline int __tsar_atomic_return_##fn(int i, atomic_t *v)     \
{                                                                   \
	int res, tmp;                                               \
	smp_mb__before_llsc();                                      \
	__asm__ __volatile__(                                       \
			"1:	ll	%[tmp], %[mem]		\n" \
			"	" #op "	%[res], %[tmp], %[imm]	\n" \
			"	sc	%[res], %[mem]		\n" \
			"	beqz	%[res], 1b		\n" \
			"	" #op "	%[res], %[tmp], %[imm]	\n" \
			: [tmp] "=&r" (tmp), [res] "=&r" (res),     \
			[mem] "+m" (v->counter)                     \
			: [imm] "Ir" (i));                          \
	smp_mb__after_llsc();                                       \
	return res;                                                 \
}

ATOMIC_RETURN(add, addu);
ATOMIC_RETURN(sub, subu);

#define atomic_add_return(i, v) __tsar_atomic_return_add(i, v)
#define atomic_sub_return(i, v) __tsar_atomic_return_sub(i, v)


/*
 * Clear or set bits in atomic variable
 */

#define ATOMIC_MASK(fn, op)                                                 \
static inline void __tsar_atomic_mask_##fn(unsigned long mask, atomic_t *v) \
{                                                                           \
	int tmp;                                                            \
	smp_mb__before_llsc();                                              \
	__asm__ __volatile__(                                               \
			"1:	ll	%[tmp], %[mem]		\n"         \
			"	" #op "	%[tmp], %[mask]		\n"         \
			"	sc	%[tmp], %[mem]		\n"         \
			"	beqz	%[tmp], 1b		\n"         \
			: [tmp] "=&r" (tmp),                                \
			[mem] "+m" (v->counter)                             \
			: [mask] "Ir" (mask));                              \
	smp_mb__after_llsc();                                               \
}

ATOMIC_MASK(clear, and);
ATOMIC_MASK(set, or);

#define atomic_clear_mask(m, v) __tsar_atomic_mask_clear(~(m), v)
#define atomic_set_mask(m, v) __tsar_atomic_mask_set(m, v)

#endif /* CONFIG_SMP */

#include <asm-generic/atomic.h>

#endif /* _ASM_TSAR_ATOMIC_H */
