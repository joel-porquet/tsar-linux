#ifndef _ASM_TSAR_EXEC_H
#define _ASM_TSAR_EXEC_H

/* align the stack pointer on a 8 bytes boundary */
#define arch_align_stack(x) ((unsigned long)(x) & ~0x7)

#endif /* _ASM_TSAR_EXEC_H */
