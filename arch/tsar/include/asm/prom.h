#ifndef _ASM_TSAR_PROM_H
#define _ASM_TSAR_PROM_H

#define HAVE_ARCH_DEVTREE_FIXUPS

extern struct boot_param_header __dtb_start; /* defined by Linux */

#ifdef CONFIG_EARLY_PRINTK
extern phys_addr_t of_early_console(void);
#endif

#endif /* _ASM_TSAR_PROM_H */
