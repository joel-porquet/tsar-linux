#ifndef _ASM_TSAR_FUTEX_H
#define _ASM_TSAR_FUTEX_H

/*
 * Futex (fast userspace mutex) API.
 */

/*
 * Atomic operations (set, add, or, andn, xor) on user memory locations.
 * Fixup code in case of error (returns -EFAULT).
 */

#define __futex_atomic_op(insn, _ret, oldval, uaddr, oparg)		\
{									\
	unsigned long tmp;						\
	__asm__ __volatile(						\
			"1:	ll	%[old], %[umem]	\n"		\
			"	" insn "		\n"		\
			"2:	sc	%[tmp], %[umem]	\n"		\
			"	beqz	%[tmp], 1b	\n"		\
			"3:				\n"		\
			".section .fixup,\"ax\"		\n" 		\
			"4:	li	%[ret], %[err]	\n" 		\
			"	j	3b		\n" 		\
			".previous			\n" 		\
			".section __ex_table,\"a\"	\n" 		\
			"	.word	1b, 4b		\n" 		\
			"	.word	2b, 4b		\n" 		\
			".previous			\n" 		\
			: [ret] "=r" (_ret), [old] "=&r" (oldval),	\
			[tmp] "=&r" (tmp), [umem] "+m" (*uaddr)		\
			: [arg] "r" (oparg), [err] "i" (-EFAULT)	\
			: "memory");					\
	smp_mb__after_llsc();						\
}

#define __futex_atomic_op_inuser(op, oldval, uaddr, oparg)			\
({										\
	int __ret = 0;								\
	switch (op) {								\
		case FUTEX_OP_SET:						\
			__futex_atomic_op("move %[tmp], %[arg]",		\
					__ret, oldval, uaddr, oparg);		\
			break;							\
		case FUTEX_OP_ADD:						\
			__futex_atomic_op("addu %[tmp], %[old], %[arg]",	\
					__ret, oldval, uaddr, oparg);		\
			break;							\
		case FUTEX_OP_OR:						\
			__futex_atomic_op("or	%[tmp], %[old], %[arg]",	\
					__ret, oldval, uaddr, oparg);		\
			break;							\
		case FUTEX_OP_ANDN:						\
			__futex_atomic_op("and	%[tmp], %[old], %[arg]",	\
					__ret, oldval, uaddr, ~oparg);		\
			break;							\
		case FUTEX_OP_XOR:						\
			__futex_atomic_op("xor	%[tmp], %[old], %[arg]",	\
					__ret, oldval, uaddr, oparg);		\
			break;							\
		default:							\
			__ret = -ENOSYS;					\
	}									\
	__ret;									\
})


/*
 * Atomic compare and exchange on user memory locations.
 * Fixup code in case of error (returns -EFAULT).
 */

#define __futex_atomic_cmpxchg_inatomic(uval, uaddr, oldval, newval)	\
({									\
	int __ret = 0;							\
	u32 val, tmp;							\
	__asm__ __volatile__(						\
			"1:	ll	%[val], %[umem]		\n"	\
			"	bne	%[val], %[old], 3f	\n"	\
			"	move	%[tmp], %[new]		\n"	\
			"2:	sc	%[tmp], %[umem]		\n"	\
			"	beqz	%[tmp], 1b		\n"	\
			"3:					\n"	\
			".section .fixup,\"ax\"			\n"	\
			"4:	li	%[ret], %[err]		\n"	\
			"	j	3b			\n"	\
			".previous				\n"	\
			".section __ex_table,\"a\"		\n"	\
			"	.word 1b, 4b			\n"	\
			"	.word 2b, 4b			\n"	\
			".previous				\n"	\
			: [ret] "=r" (__ret), [val] "=&r" (val),	\
			[tmp] "=&r" (tmp), [umem]"+m" (*uaddr)		\
			: [old] "r" (oldval), [new] "r" (newval),	\
			[err] "i" (-EFAULT)				\
			: "memory");					\
	*uval = val;							\
	__ret;								\
})

#include <asm-generic/futex.h>

#endif /* _ASM_TSAR_FUTEX_H */
