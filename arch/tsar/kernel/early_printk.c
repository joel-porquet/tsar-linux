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
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/types.h>

#include <asm/io.h>

/* define a few things about SoCLib vci_multi_tty */
#define VCI_TTY_WRITE	0x0	/* offset for writing operation */
#define VCI_TTY_SIZE	0x10	/* size of the tty segment */

static void __iomem *vci_tty_virt_base;

static void early_tty_putchar(const char c)
{
	/* SoCLib vci_multi_tty register map (short) */

	__raw_writel(c, vci_tty_virt_base + VCI_TTY_WRITE);

}
static void early_tty_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			early_tty_putchar('\r');
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

static char *stdout_path __initdata;

/*
 * This function is supposed to receive all the nodes of the device tree, in
 * order. First it must find the chosen node (to find the "linux,stdout-path"
 * property), and then the corresponding tty node.
 * WARNING: It means that it is of the utmost importance that the chosen node
 * appears before the tty node in the DTS/DTB!
 */
static int __init early_init_dt_scan_chosen_tty(unsigned long node, const char *uname,
		int depth, void *data)
{
	phys_addr_t *tty_base_addr = data;
	unsigned long l;
	char *p;

	pr_debug("search \"chosen\", depth: %d, uname: %s\n", depth, uname);

	/* first of all, find the chosen node */
	if (depth == 1 &&
			(strcmp(uname, "chosen") == 0 ||
			 strcmp(uname, "chosen@0") == 0)) {
		p = of_get_flat_dt_prop(node, "linux,stdout-path", &l);
		if ((p != NULL) && (l > 0)) {
			/* store pointer to stdout-path */
			pr_debug("found linux,stdout-path property: %s\n", p);
			stdout_path = p;
		}
	}

	/* later (hopefully, when stdout_path has been found), find the
	 * corresponding node */
	if (stdout_path && strstr(stdout_path, uname)) {
		p = of_get_flat_dt_prop(node, "compatible", &l);
		pr_debug("linux,stdout-path compatible string: %s\n", p);

		if (strcmp(p, "soclib,vci_multi_tty") == 0) {
			void *a;
			a = of_get_flat_dt_prop(node, "reg", &l);
			*tty_base_addr = be64_to_cpup(a);
			return 1;
		}
	}

	/* return 0 until found */
	return 0;
}

/* this function works while the DTB is still unflattened */
static int __init of_early_console(phys_addr_t *console_addr)
{
	return of_scan_flat_dt(early_init_dt_scan_chosen_tty, console_addr);
}

static int __init setup_early_printk(char *buf)
{
	phys_addr_t vci_tty_base_addr;

	/* already initialized */
	if (early_console)
		return 0;

	/* setup the physical address of the early console device */
	if (!of_early_console(&vci_tty_base_addr))
		/* no tty is available... */
		return 0;

	/* map the tty */
	vci_tty_virt_base = ioremap_nocache(vci_tty_base_addr, VCI_TTY_SIZE);

	BUG_ON(!vci_tty_virt_base);

	early_console = &early_tty_console;
	register_console(early_console);

	return 0;
}

early_param("earlyprintk", setup_early_printk);
