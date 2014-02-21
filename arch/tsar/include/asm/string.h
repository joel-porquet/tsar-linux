#ifndef _ASM_TSAR_STRING_H
#define _ASM_TSAR_STRING_H

#include <asm/posix_types.h>

/* let's provide a few optimized string functions */

#define __HAVE_ARCH_MEMSET
extern void *memset(void *, int, __kernel_size_t);

#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *, const void *, __kernel_size_t);

#define __HAVE_ARCH_MEMMOVE
extern void *memmove(void *, const void *, __kernel_size_t);

#endif /* _ASM_TSAR_STRING_H */
