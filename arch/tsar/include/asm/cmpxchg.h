#ifndef _ASM_TSAR_CMPXCHG_H
#define _ASM_TSAR_CMPXCHG_H

#include <asm/barrier.h>

/*
 * Definition of xchg function.
 *
 * Atomically assign a new value to a memory location, and returns the previous
 * value. On TSAR, we use ll/sc which only supports 32-bits accesses.
 */

static inline unsigned long __xchg_u32(volatile void *m, unsigned long val)
{
	unsigned long prev, tmp;

	smp_mb__before_llsc();

	__asm__ __volatile__(
			"1:	ll	%[prev], %[mem]		\n"
			"	move	%[tmp], %[val]		\n"
			"	sc	%[tmp], %[mem]		\n"
			"	beqz	%[tmp], 1b		\n"
			: [tmp] "=&r" (tmp), [prev] "=&r" (prev),
			[mem] "+m" (*(volatile int *)m)
			: [val] "r" (val)
			: "memory");

	smp_mb__after_llsc();
	return prev;
}

extern void __xchg_called_with_bad_pointer(void);

static inline unsigned long __xchg(unsigned long x,
		volatile void *ptr, int size)
{
	switch (size) {
		case 4:
			return __xchg_u32(ptr, x);
		default:
			__xchg_called_with_bad_pointer();
			return x;
	}
}

#define xchg(ptr, x) \
	((__typeof__(*(ptr))) __xchg((unsigned long)(x), (ptr), sizeof(*(ptr))))

/*
 * Definition of cmpxchg* functions.
 *
 * Atomically compare and exchange: if the targeted memory location contains a
 * certain value, then atomically assign a new value to it. It returns the
 * previous value.
 *
 * The "local" versions restrict their scope to one processor only, while the
 * general versions work with respect to multiprocessor (ie include memory
 * barriers).
 */

#define __HAVE_ARCH_CMPXCHG 1

static inline unsigned long __cmpxchg_u32(volatile void *m,
		unsigned long old, unsigned long new)
{
	unsigned long prev, tmp;
	__asm__ __volatile__(
			"1:	ll	%[prev], %[mem]		\n"
			"	bne	%[prev], %[old], 2f	\n"
			"	move	%[tmp], %[new]		\n"
			"	sc	%[tmp], %[mem]		\n"
			"	beqz	%[tmp], 1b		\n"
			"2:					\n"
			: [tmp] "=&r" (tmp), [prev] "=&r" (prev),
			[mem] "+m" (*(volatile int *)m)
			: [old] "r" (old), [new] "r" (new)
			: "memory");
	return prev;
}

extern void __cmpxchg_called_with_bad_pointer(void);

static inline unsigned long __cmpxchg(volatile void *ptr,
		unsigned long old, unsigned long new, int size)
{
	switch (size) {
		case 4:
			return __cmpxchg_u32(ptr, old, new);
		default:
			__cmpxchg_called_with_bad_pointer();
			return old;
	}
}

/* the normal version includes memory barriers */
#define cmpxchg(ptr, old, new)				\
({							\
 	__typeof__(*(ptr)) __res;			\
	smp_mb__before_llsc();				\
	__res = (__typeof__(*(ptr)))__cmpxchg((ptr),	\
		(unsigned long)(old),			\
		(unsigned long)(new),			\
		sizeof(*(ptr)));			\
	smp_mb__after_llsc();				\
	__res;						\
})

#define cmpxchg_local(ptr, old, new)		\
	(__typeof__(*(ptr)))__cmpxchg((ptr),	\
			(unsigned long)old,	\
			(unsigned long)new,	\
			sizeof(*(ptr)))

#define cmpxchg64(ptr, o, n)			\
({						\
	BUILD_BUG_ON(sizeof(*(ptr)) != 8);	\
	cmpxchg((ptr), (o), (n));		\
 })

/* use the generic version for local 64-bits version */
#include <asm-generic/cmpxchg-local.h>
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#endif /* _ASM_TSAR_CMPXCHG_H */
