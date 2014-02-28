#ifndef _ASM_TSAR_BITOPS_H
#define _ASM_TSAR_BITOPS_H

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>

#include <asm/barrier.h>

/* clear_bit can be used for locking purpose:
 * - we need to execute a hardware sync before to commit the pending writes to
 *   memory
 * - clear_bit is performed with ll/sc, and sc is visible immediately so we
 *   just need a compiler barrier after it
 */
#define smp_mb__before_clear_bit()	smp_mb__before_llsc()
#define smp_mb__after_clear_bit()	smp_mb__after_llsc()

/*
 * For certain bitops, we can provide optimized asm routines:
 * - set_bit, clear_bit, change_bit
 * - test_and_set_bit, test_and_clear_bit, test_and_change_bit
 * - __ffs, fls, __fls, ffs
 *
 * Mostly adapted from MIPS support
 */

/*
 * #include <asm-generic/bitops/atomic.h>
 */

/*
 * Bitop functions: set_bit, clear_bit and change_bit
 */

#define DEFINE_BITOP(fn, op)                                 \
static inline void __tsar_##fn(unsigned long mask,           \
		volatile unsigned long *_p)                  \
{                                                            \
	unsigned long tmp;                                   \
	unsigned long *p = (unsigned long *) _p;             \
	__asm__ __volatile(                                  \
			".set push			\n"  \
			".set noreorder			\n"  \
			"1:	ll	%[tmp], %[mem]	\n"  \
			"	" #op "	%[tmp], %[mask]	\n"  \
			"	sc	%[tmp], %[mem]	\n"  \
			".set pop			\n"  \
			"	beqz	%[tmp], 1b	\n"  \
			: [tmp] "=&r" (tmp), [mem] "+m" (*p) \
			: [mask] "r" (mask));                \
}

DEFINE_BITOP(set_bit, or);
DEFINE_BITOP(clear_bit, and);
DEFINE_BITOP(change_bit, xor);

static inline void set_bit(int nr, volatile unsigned long *addr)
{
	__tsar_set_bit(BIT_MASK(nr), addr + BIT_WORD(nr));
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
	__tsar_clear_bit(~BIT_MASK(nr), addr + BIT_WORD(nr));
}

static inline void change_bit(int nr, volatile unsigned long *addr)
{
	__tsar_change_bit(BIT_MASK(nr), addr + BIT_WORD(nr));
}

/*
 * Testop functions: test_and_set_bit, test_and_clear_bit, test_and_change_bit
 */

#define DEFINE_TESTOP(fn, op)                                       \
static inline unsigned long __tsar_##fn(unsigned long mask,         \
		volatile unsigned long *_p)                         \
{                                                                   \
	unsigned long old, res;                                     \
	unsigned long *p = (unsigned long *) _p;                    \
	smp_mb__before_llsc();                                      \
	__asm__ __volatile(                                         \
			".set push				\n" \
			".set noreorder				\n" \
			"1:	ll	%[old], %[mem]		\n" \
			"	" #op "	%[res], %[old], %[mask]	\n" \
			"	sc	%[res], %[mem]		\n" \
			".set pop				\n" \
			"	beqz	%[res], 1b		\n" \
			: [old] "=&r" (old), [res] "=&r" (res),     \
			[mem] "+m" (*p)                             \
			: [mask] "r" (mask)                         \
			: "memory");                                \
	smp_mb__after_llsc();                                       \
	return old;                                                 \
}

DEFINE_TESTOP(test_and_set_bit, or);
DEFINE_TESTOP(test_and_clear_bit, and);
DEFINE_TESTOP(test_and_change_bit, xor);

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long old = __tsar_test_and_set_bit(BIT_MASK(nr), addr + BIT_WORD(nr));
	return (old & BIT_MASK(nr)) != 0;
}

static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long old = __tsar_test_and_clear_bit(~BIT_MASK(nr), addr + BIT_WORD(nr));
	return (old & BIT_MASK(nr)) != 0;
}

static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long old = __tsar_test_and_change_bit(BIT_MASK(nr), addr + BIT_WORD(nr));
	return (old & BIT_MASK(nr)) != 0;
}

/*
 * #include <asm-generic/bitops/__fls.h>
 */
static inline unsigned long __fls(unsigned long word)
{
	int num;
	__asm__(
			"clz	%0, %1\n"
			: "=r" (num)
			: "r" (word)
	       );
	return 31 - num;
}

/*
 * #include <asm-generic/bitops/__ffs.h>
 */
static inline unsigned long __ffs(unsigned long word)
{
	return __fls(word & -word);
}

/*
 * #include <asm-generic/bitops/fls.h>
 */
static inline int fls(int x)
{
	__asm__(
			"clz	%0, %1"
			: "=r" (x)
			: "r" (x)
	       );
	return 32 - x;
}

/*
 * #include <asm-generic/bitops/ffs.h>
 */
static inline int ffs(int word)
{
	if (!word)
		return 0;
	return fls(word & -word);
}

#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/fls64.h>
#include <asm-generic/bitops/find.h>

#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>

#include <asm-generic/bitops/sched.h>

#include <asm-generic/bitops/non-atomic.h>
#include <asm-generic/bitops/le.h>
#include <asm-generic/bitops/ext2-atomic.h>

#endif /* _ASM_TSAR_BITOPS_H */
