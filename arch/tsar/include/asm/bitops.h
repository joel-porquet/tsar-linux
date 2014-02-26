#ifndef _ASM_TSAR_BITOPS_H
#define _ASM_TSAR_BITOPS_H

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <asm/barrier.h>

#ifndef smp_mb__before_clear_bit
#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	smp_mb()
#endif

/*
 * For certain bitops, we can provide optimized asm routines:
 * - __ffs, fls, __fls, ffs
 * - test_and_set_bit, test_and_clear_bit, test_and_change_bit
 * - set_bit, clear_bit, change_bit
 *
 * Mostly adapted from MIPS support
 */

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

/*
 * #include <asm-generic/bitops/atomic.h>
 */
static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old, res;

	smp_mb();

	__asm__ __volatile__(
			".set push				\n"
			".set noreorder				\n"
			"1:	ll	%[old], %[mem]		\n"
			"	or	%[res], %[old], %[mask]	\n"
			"	sc	%[res], %[mem]		\n"
			".set pop				\n"
			"	beqz	%[res], 1b		\n"
			: [old] "=&r" (old), [res] "=&r" (res),
			[mem] "+m" (*p)
			: [mask] "r" (mask)
			: "memory");

	smp_mb();

	return (old & mask) != 0;
}

static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old, res;

	smp_mb();

	__asm__ __volatile__(
			".set push				\n"
			".set noreorder				\n"
			"1:	ll	%[old], %[mem]		\n"
			"	or	%[res], %[old], %[mask]	\n"
			"	xor	%[res], %[mask]		\n"
			"	sc	%[res], %[mem]		\n"
			".set pop				\n"
			"	beqz	%[res], 1b		\n"
			: [old] "=&r" (old), [res] "=&r" (res),
			[mem] "+m" (*p)
			: [mask] "r" (mask)
			: "memory");

	smp_mb();

	return (old & mask) != 0;
}

static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old, res;

	smp_mb();

	__asm__ __volatile__(
			".set push				\n"
			".set noreorder				\n"
			"1:	ll	%[old], %[mem]		\n"
			"	xor	%[res], %[old], %[mask]	\n"
			"	sc	%[res], %[mem]		\n"
			".set pop				\n"
			"	beqz	%[res], 1b		\n"
			: [old] "=&r" (old), [res] "=&r" (res),
			[mem] "+m" (*p)
			: [mask] "r" (mask)
			: "memory");

	smp_mb();

	return (old & mask) != 0;
}

static inline void set_bit(int nr, volatile unsigned long *addr)
{
	test_and_set_bit(nr, addr);
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
	test_and_clear_bit(nr, addr);
}

static inline void change_bit(int nr, volatile unsigned long *addr)
{
	test_and_change_bit(nr, addr);
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
