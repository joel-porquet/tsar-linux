#ifndef _ASM_TSAR_VGA_H
#define _ASM_TSAR_VGA_H

#include <asm/io.h>

#define VGA_MAP_MEM(x,s) ((unsigned long)ioremap(x, s))

#include <asm-generic/vga.h>

#endif /* _ASM_TSAR_VGA_H */
