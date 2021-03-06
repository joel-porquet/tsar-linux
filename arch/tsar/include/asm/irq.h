#ifndef _TSAR_ASM_IRQ_H
#define _TSAR_ASM_IRQ_H

#include <asm/ptrace.h>

/* IRQ handler function template */
#define HANDLE_IRQ(n) void (n)(struct pt_regs *regs)
typedef HANDLE_IRQ(handle_irq_t);

extern handle_irq_t *handle_irq_icu;

extern void set_handle_irq(handle_irq_t *);

#include <asm-generic/irq.h>

void init_IRQ(void);

#endif /* _TSAR_ASM_IRQ_H */
