/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/prom.h>

static void __iomem *vci_tty_virt_base;

static void early_tty_putchar(const char c)
{
	/* SoCLib vci_multi_tty register map (short) */
	#define VCI_TTY_WRITE 0x0

	__raw_writel(c, vci_tty_virt_base + VCI_TTY_WRITE);

}
static void early_tty_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		//if (*s == '\n')
		//	early_tty_putchar('\r');
		early_tty_putchar(*s);
		s++;
	}
}

static struct console early_tty_console = {
	.name	= "early_tty_cons",
	.write	= early_tty_write,
	/* boot console (and print everything we miss from the start) */
	.flags	= CON_BOOT | CON_PRINTBUFFER,
	.index	= -1
};

void __init early_printk_init(void)
{
	phys_addr_t vci_tty_base_addr = 0;

	if (early_console)
		return;

	/* get the physical base address of the tty from the device tree */
	vci_tty_base_addr = of_early_console();

	if (!vci_tty_base_addr)
		/* no tty is available... */
		return;

	/* map the tty */
	vci_tty_virt_base = ioremap_nocache(vci_tty_base_addr, 0xc);

	if (!vci_tty_virt_base)
		panic("Failed to remap early_printk memory\n");

	early_console = &early_tty_console;
	register_console(early_console);
}
