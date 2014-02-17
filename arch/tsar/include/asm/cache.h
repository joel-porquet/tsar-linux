#ifndef _ASM_TSAR_CACHE_H
#define _ASM_TSAR_CACHE_H

#include <linux/const.h>

#define L1_CACHE_SHIFT _AC(CONFIG_TSAR_L1_CACHE_SHIFT, UL)
#define L1_CACHE_BYTES (1 << L1_CACHE_SHIFT)

#endif /* _ASM_TSAR_CACHE_H */
