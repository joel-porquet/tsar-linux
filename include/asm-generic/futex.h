#ifndef _ASM_GENERIC_FUTEX_H
#define _ASM_GENERIC_FUTEX_H

#include <linux/futex.h>
#include <linux/uaccess.h>
#include <asm/errno.h>

#ifndef __futex_atomic_op_inuser
#define __futex_atomic_op_inuser(op, oldval, uaddr, oparg)	\
({								\
	int __ret;						\
	switch (op) {						\
	case FUTEX_OP_SET:					\
	case FUTEX_OP_ADD:					\
	case FUTEX_OP_OR:					\
	case FUTEX_OP_ANDN:					\
	case FUTEX_OP_XOR:					\
	default:						\
		__ret = -ENOSYS;				\
	}							\
	__ret;							\
})
#endif

static inline int
futex_atomic_op_inuser (int encoded_op, u32 __user *uaddr)
{
	int op = (encoded_op >> 28) & 7;
	int cmp = (encoded_op >> 24) & 15;
	int oparg = (encoded_op << 8) >> 20;
	int cmparg = (encoded_op << 20) >> 20;
	int oldval = 0, ret;
	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
		oparg = 1 << oparg;

	if (! access_ok (VERIFY_WRITE, uaddr, sizeof(u32)))
		return -EFAULT;

	pagefault_disable();

	ret = __futex_atomic_op_inuser(op, oldval, uaddr, oparg);

	pagefault_enable();

	if (!ret) {
		switch (cmp) {
		case FUTEX_OP_CMP_EQ: ret = (oldval == cmparg); break;
		case FUTEX_OP_CMP_NE: ret = (oldval != cmparg); break;
		case FUTEX_OP_CMP_LT: ret = (oldval < cmparg); break;
		case FUTEX_OP_CMP_GE: ret = (oldval >= cmparg); break;
		case FUTEX_OP_CMP_LE: ret = (oldval <= cmparg); break;
		case FUTEX_OP_CMP_GT: ret = (oldval > cmparg); break;
		default: ret = -ENOSYS;
		}
	}
	return ret;
}

#ifndef __futex_atomic_cmpxchg_inatomic
#define __futex_atomic_cmpxchg_inatomic(uval, uaddr, oldval, newval)	\
({									\
	int __res = -ENOSYS;						\
	__res;								\
})
#endif

static inline int
futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
			      u32 oldval, u32 newval)
{
	if (!access_ok(VERIFY_WRITE, uaddr, sizeof(u32)))
		return -EFAULT;

	return __futex_atomic_cmpxchg_inatomic(uval, uaddr, oldval, newval);
}

#endif
