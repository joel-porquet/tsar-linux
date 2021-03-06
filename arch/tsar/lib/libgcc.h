#ifndef _ASM_LIBGCC_H
#define _ASM_LIBGCC_H

#include <asm/byteorder.h>

typedef int word_type __attribute__ ((mode (__word__)));

struct DWstruct {
	int low, high;
};

typedef union {
	struct DWstruct s;
	long long ll;
} DWunion;

#endif /* _ASM_LIBGCC_H */
