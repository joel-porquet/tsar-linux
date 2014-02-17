#ifndef _ASM_TSAR_PROM_H
#define _ASM_TSAR_PROM_H

#define HAVE_ARCH_DEVTREE_FIXUPS

extern void tsar_device_tree_early_init(void);

#ifdef CONFIG_EARLY_PRINTK
extern phys_addr_t of_early_console(void);
#endif

#endif /* _ASM_TSAR_PROM_H */
