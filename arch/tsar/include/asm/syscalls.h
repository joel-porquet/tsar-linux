#ifndef _ASM_TSAR_SYSCALLS_H
#define _ASM_TSAR_SYSCALLS_H

#include <linux/compiler.h>
#include <linux/linkage.h>

#include <asm-generic/syscalls.h>

/*
 * add our own arch specific syscalls
 */
asmlinkage long sys_set_thread_area(unsigned long);

#endif /* _ASM_TSAR_SYSCALLS_H */
