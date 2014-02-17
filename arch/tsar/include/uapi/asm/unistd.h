#define __ARCH_HAVE_MMU

/* clone seems to be the only necessary syscall for creating user threads: fork
 * and vfork can be emulated with it - see libc for more details */
#define __ARCH_WANT_SYS_CLONE

#define sys_mmap2 sys_mmap_pgoff

/* libc expects llseek with another name */
#define __NR__llseek __NR_llseek

#include <asm-generic/unistd.h>

/*
 * add our own arch specific syscalls
 */
#define __NR_set_thread_area __NR_arch_specific_syscall
__SYSCALL(__NR_set_thread_area, sys_set_thread_area)
