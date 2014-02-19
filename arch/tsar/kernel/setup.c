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
#include <linux/ioport.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/of_fdt.h>
#include <linux/printk.h>
#include <linux/seq_file.h>

#include <asm/io.h>
#include <asm/meminfo.h>
#include <asm/sections.h>
#include <asm/tsar_setup.h>

struct meminfo meminfo;

static struct resource kernel_code_resource = { .name = "Kernel code", };
static struct resource kernel_data_resource = { .name = "Kernel data", };
static struct resource kernel_bss_resource = { .name = "Kernel bss", };

extern struct boot_param_header __dtb_start; /* defined by Linux */
void *dtb_start = &__dtb_start;

int __init meminfo_add_membank(phys_addr_t start, phys_addr_t size)
{
	struct membank *bank = &meminfo.bank[meminfo.nr_banks];

	if (meminfo.nr_banks >= NR_MEM_BANKS) {
		pr_crit("NR_BANKS too low, ignoring memory at 0x%08llx\n",
				(long long)start);
		return -EINVAL;
	}

	/* start and size must be page aligned:
	 * - start is rounded up
	 * - size is rounded down */
	size -= PAGE_ALIGN(start) - start;

	membank_phys_start(bank) = PAGE_ALIGN(start);
	membank_phys_size(bank) = size & PAGE_MASK;

	/* check the size of this new region has non-zero size */
	if (membank_phys_size(bank) == 0)
		return -EINVAL;

	meminfo.nr_banks++;
	return 0;
}

static void __init resource_init(void)
{
	int i;

	kernel_code_resource.start = __pa(_stext);
	kernel_code_resource.end = __pa(_etext) - 1;
	kernel_code_resource.flags = IORESOURCE_BUSY | IORESOURCE_MEM;

	kernel_data_resource.start = __pa(_sdata);
	kernel_data_resource.end = __pa(_edata) - 1;
	kernel_data_resource.flags = IORESOURCE_BUSY | IORESOURCE_MEM;

	kernel_bss_resource.start = __pa(__bss_start);
	kernel_bss_resource.end = __pa(__bss_stop) - 1;
	kernel_bss_resource.flags = IORESOURCE_BUSY | IORESOURCE_MEM;

	/* XXX: should we use for_each_memblock instead? */
	for_each_membank(i, &meminfo) {
		struct membank *bank = &meminfo.bank[i];

		/* XXX: should we discard highmem memory banks as in MIPS? */
		struct resource *res;
		res = __va(memblock_alloc(sizeof(struct resource), SMP_CACHE_BYTES));

		/* signal the memory bank to the resource manager */
		res->name = "System RAM";
		res->start = bank->start;
		res->end = bank->start + bank->size - 1;
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;

		request_resource(&iomem_resource, res);

		/* let the resource manager insert those segments into the
		 * right memory bank */
		request_resource(res, &kernel_code_resource);
		request_resource(res, &kernel_data_resource);
		request_resource(res, &kernel_bss_resource);
	}
}

void __init setup_arch(char **cmdline_p)
{
	/* early parsing of the device tree to setup the machine:
	 * - memory banks (meminfo structure)
	 * - bootargs (boot_command_line definition)
	 */
	early_init_devtree(dtb_start);

	/* make it possible to have virtual mappings before memory and proper
	 * ioremap are up: necessary for earlyprintk and/or earlycon */
	ioremap_fixed_early_init();

	/* use tty as an early console */
	early_printk_init();

	/* parse early param of boot_command_line,
	 * such as 'earlycon' for example */
	parse_early_param();

	/* setup memory:
	 * - memblock
	 * - zones
	 */
	tsar_memory_init();

	paging_init();

	resource_init();

	/* finish parsing the device tree */
	unflatten_and_copy_device_tree();

	/* give boot_command_line back to init/main.c */
	*cmdline_p = boot_command_line;

	/* configure a virtual terminal */
#if defined(CONFIG_VT)
	conswitchp = &dummy_con;
#endif
}

static int show_cpuinfo(struct seq_file *m, void *v)
{
	unsigned long n = (unsigned long) v - 1;

#ifdef CONFIG_SMP
	if (!cpu_online(n))
		return 0;
#endif

	seq_printf(m, "processor\t\t: %ld\n", n);
	seq_printf(m, "\n");

	return 0;
}

static void *c_start(struct seq_file *m, loff_t *pos)
{
	unsigned long i = *pos;

	return (i < NR_CPUS) ? (void*)(i + 1) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= show_cpuinfo,
};
