#ifndef _ASM_TSAR_TRAPS_H
#define _ASM_TSAR_TRAPS_H

#include <asm/ptrace.h>

/*
 * exception management
 */
extern void general_exception_vector(void);
extern void handle_reserved(void);
extern void handle_int(void);
extern void handle_ade(void);
extern void handle_ibe(void);
extern void handle_dbe(void);
extern void handle_sys(void);
extern void handle_bp(void);
extern void handle_ri(void);
extern void handle_cpu(void);
extern void handle_ov(void);
extern void handle_tr(void);
//extern void handle_fpe(void);

extern void do_page_fault(struct pt_regs*, unsigned long,
		unsigned int, unsigned int);

#endif /* _ASM_TSAR_TRAPS_H */
