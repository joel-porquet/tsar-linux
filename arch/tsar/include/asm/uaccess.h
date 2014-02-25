#ifndef _ASM_TSAR_UACCESS_H
#define _ASM_TSAR_UACCESS_H

#include <linux/compiler.h>
#include <linux/stddef.h>
#include <linux/types.h>

/* address space upper limit of the current thread */
#define user_addr_max() (get_fs().seg)

/*
 * Define arch-specific __access_ok(), as suggested by asm-generic.
 *
 * Checks if a block of memory is a valid user space address.
 */

/* taken from x86 */
static inline bool
__range_not_ok(unsigned long addr, unsigned long size, unsigned long limit)
{
	/* If we have used "sizeof()" for the size,
	 * we know it won't overflow the limit (but
	 * it might overflow the 'addr', so it's
	 * important to subtract the size from the
	 * limit, not add it to the address).
	 */
	if (__builtin_constant_p(size))
		return (addr > (limit - size));

	/* be careful about overflow */
	addr += size;
	if (addr < size)
		return true;
	return addr > limit;
}

#define __access_ok(addr, size) \
	likely(!__range_not_ok(addr, size, user_addr_max()))


/*
 * Define arch-specific __copy_from_user() and __copy_to_user(), as suggested
 * by asm-generic.
 *
 * In our case, use a unique __copy_tofrom_user() asm function that checks both
 * reads and writes in case either the source or the destination belongs to
 * user space.
 *
 * It returns 0 on success, or the number of NOT copied bytes in case of
 * error.
 */

extern unsigned long __must_check
__copy_tofrom_user(void *to, const void *from, unsigned long size);

#define __copy_from_user(to, from, size) \
	__copy_tofrom_user(to, from, size)
#define __copy_to_user(to, from, size) \
	__copy_tofrom_user(to, from, size)


/*
 * Define arch-specific __put_user_fn() and __get_user_fn().
 *
 * Those macros are single byte/hword/word/dword copies which support
 * exceptions (in case the access into userspace causes a page fault).
 *
 * They return 0 on success, -EFAULT for error.
 */

/* int __put_user_fn(size_t size, void __user *ptr, void *x); */
#define __put_user_fn(size, ptr, x)                             \
({                                                              \
	int retval = 0; /* success by default */                \
	switch (size) {                                         \
	case 1: __put_user_asm(*(x), ptr, "sb", retval); break; \
	case 2: __put_user_asm(*(x), ptr, "sh", retval); break; \
	case 4: __put_user_asm(*(x), ptr, "sw", retval); break; \
	case 8: __put_user_asm2(*(x), ptr, retval); break;      \
	}                                                       \
	retval;                                                 \
})

/* err (i.e. ret) is already set to 0 above. Only in case of error, fixup code
 * will set it to -EFAULT.
 *
 * __put_user_asm handles byte/half/word access.
 * __put_user_asm2 handles dword access.
 */
#define __put_user_asm(x, ptr, op, err)                     \
	__asm__ __volatile__(                               \
			"1:	"op"	%1, (%2)\n"         \
			"2:\n"                              \
			".section .fixup,\"ax\"\n"          \
			"3:	li	%0, %3\n"           \
			"	j	2b\n"               \
			".previous\n"                       \
			".section __ex_table,\"a\"\n"       \
			"	.word	1b, 3b\n"           \
			".previous\n"                       \
			: "+r" (err)                        \
			: "r" (x), "r" (ptr), "i" (-EFAULT) \
			)
#define __put_user_asm2(x, ptr, err)                        \
	__asm__ __volatile__(                               \
			"1:	sw	%1, (%2)\n"         \
			"2:	sw	%D1, 4(%2)\n"       \
			"3:\n"                              \
			".section .fixup,\"ax\"\n"          \
			"4:	li	%0, %3\n"           \
			"	j	2b\n"               \
			".previous\n"                       \
			".section __ex_table,\"a\"\n"       \
			"	.word	1b, 4b\n"           \
			"	.word	2b, 4b\n"           \
			".previous\n"                       \
			: "+r" (err)                        \
			: "r" (x), "r" (ptr), "i" (-EFAULT) \
			)

/* int __get_user_fn(size_t size, const void __user *ptr, void *x) */
#define __get_user_fn(size, ptr, x)                             \
({                                                              \
	int retval = 0; /* success by default */                \
	switch (size) {                                         \
	case 1: __get_user_asm(*(x), ptr, "lb", retval); break; \
	case 2: __get_user_asm(*(x), ptr, "lh", retval); break; \
	case 4: __get_user_asm(*(x), ptr, "lw", retval); break; \
	case 8: __get_user_asm2(*(x), ptr, retval); break;      \
	}                                                       \
	retval;                                                 \
})

/* err (i.e. ret) is already set to 0 above. Only in case of error, fixup code
 * will set it to -EFAULT.
 *
 * __get_user_asm handles byte/half/word access.
 * __get_user_asm2 handles dword access.
 */
#define __get_user_asm(x, ptr, op, err)               \
	__asm__ __volatile__(                         \
			"1:	"op"	%1, (%2)\n"   \
			"2:\n"                        \
			".section .fixup,\"ax\"\n"    \
			"3:	li	%0, %3\n"     \
			"	j	2b\n"         \
			".previous\n"                 \
			".section __ex_table,\"a\"\n" \
			"	.word	1b, 3b\n"     \
			".previous\n"                 \
			: "+r" (err), "=r" (x)        \
			: "r" (ptr), "i" (-EFAULT)    \
			)
#define __get_user_asm2(x, ptr, err)                  \
	__asm__ __volatile__(                         \
			"1:	lw	%1, (%2)\n"   \
			"2:	lw	%D1, 4(%2)\n" \
			"3:\n"                        \
			".section .fixup,\"ax\"\n"    \
			"4:	li	%0, %3\n"     \
			"	j	2b\n"         \
			".previous\n"                 \
			".section __ex_table,\"a\"\n" \
			"	.word	1b, 4b\n"     \
			"	.word	2b, 4b\n"     \
			".previous\n"                 \
			: "+r" (err), "=r" (x)        \
			: "r" (ptr), "i" (-EFAULT)    \
			)


/*
 * Define arch-specific __clear_user().
 *
 * It returns 0 on success, or the number of NOT copied bytes in case of
 * error.
 */
extern unsigned long __must_check
__clear_user(void __user *to, unsigned long n);
#define __clear_user __clear_user


#include <asm-generic/uaccess.h>

#endif /* _ASM_TSAR_UACCESS_H */
