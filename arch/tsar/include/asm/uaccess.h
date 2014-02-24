#ifndef _ASM_TSAR_UACCESS_H
#define _ASM_TSAR_UACCESS_H

#define __kernel_ok		(segment_eq(get_fs(), KERNEL_DS))
#define __user_ok(addr, size)	(((size) <= get_fs().seg) && ((addr) <= (get_fs().seg - (size))))
#define __access_ok(addr, size)	(__kernel_ok || __user_ok((addr), (size)))

#include <asm-generic/uaccess.h>

#endif /* _ASM_TSAR_UACCESS_H */
