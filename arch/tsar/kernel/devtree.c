#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/memblock.h>
#include <linux/types.h>

#include <asm/meminfo.h>
#include <asm/page.h>
#include <asm/prom.h>

int __init early_init_dt_scan_memory_arch(unsigned long node,
		const char *uname, int depth,
		void *data)
{
	/* do nothing but call the reference function */
	return early_init_dt_scan_memory(node, uname, depth, data);
}

void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	meminfo_add_membank(base, size);
}

int __init early_init_dt_scan_model(unsigned long node,	const char *uname,
		int depth, void *data)
{
	if (!depth) {
		char *model = of_get_flat_dt_prop(node, "model", NULL);

		if (model)
			pr_info("Model: %s\n", model);
	}
	return 0;
}

#ifdef CONFIG_BLK_DEV_INITRD
void __init early_init_dt_setup_initrd_arch(u64 start, u64 end)
{
	pr_err("%s(%llx, %llx)\n", __func__, start, end);
}
#endif

#ifdef CONFIG_EARLY_PRINTK
static char *stdout_path __initdata;

static int __init early_init_dt_scan_chosen_tty(unsigned long node, const char *uname,
		int depth, void *data)
{
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
			phys_addr_t *base_addr;
			base_addr = of_get_flat_dt_prop(node, "reg", &l);
			return be32_to_cpup(base_addr);
		}
	}

	/* return 0 until found */
	return 0;
}

/* this function returns the physical address of the tty, to allow for early
 * prink, according to the "chosen" node */
phys_addr_t __init of_early_console()
{
	return of_scan_flat_dt(early_init_dt_scan_chosen_tty, NULL);
}
#endif

void __init early_init_devtree(void *params)
{
	if (!early_init_dt_scan(params)) {
		pr_err("Scanning DTB failed!\n");
		return;
	}

	/* get and display machine model */
	of_scan_flat_dt(early_init_dt_scan_model, NULL);
}

int __init tsar_device_probe(void)
{
	if (!of_have_populated_dt())
		panic("Device tree not present!");

	return of_platform_populate(NULL, of_default_bus_match_table,
			NULL, NULL);
}
arch_initcall(tsar_device_probe);
