#ifndef _UAPI_ASM_TSAR_SIGCONTEXT_H
#define _UAPI_ASM_TSAR_SIGCONTEXT_H

#include <asm/ptrace.h>

struct sigcontext {
	struct user_regs_struct regs;
	unsigned long oldmask;
};

#endif /* _UAPI_ASM_TSAR_SIGCONTEXT_H */
