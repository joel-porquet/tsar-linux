#ifndef _ASM_TSAR_SETUP_H
#define _ASM_TSAR_SETUP_H

/* defines COMMAND_LINE_SIZE */
#include <asm-generic/setup.h>

/*
 * prototypes for at boot time (e.g. setup_arch())
 */

extern void memory_init(void);

#endif /* _ASM_TSAR_SETUP_H */
