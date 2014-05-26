#ifndef _ASM_TSAR_SWITCH_TO_H
#define _ASM_TSAR_SWITCH_TO_H

#include <asm-generic/switch_to.h>

#define finish_arch_switch(prev)                                      \
	do {                                                          \
		write_c0_userlocal(current_thread_info()->tp_value);  \
	} while(0)


#endif /* _ASM_TSAR_SWITCH_TO_H */
