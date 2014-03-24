/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <asm/thread_info.h>
#include <asm/page.h>
#include <asm/cache.h>

#define LOAD_OFFSET PAGE_OFFSET

/* make sure swapper_pg_dir will appear first in .bss section */
#define BSS_FIRST_SECTIONS *(.bss..swapper_pg_dir)

#include <asm-generic/vmlinux.lds.h>

#define IDMAP_TEXT				\
	ALIGN_FUNCTION();			\
	VMLINUX_SYMBOL(__idmap_text_start) = .;	\
	*(.idmap.text)				\
	VMLINUX_SYMBOL(__idmap_text_end) = .;

#undef mips
#define mips mips
OUTPUT_ARCH(mips)
ENTRY(phys_kernel_entry)

jiffies = jiffies_64;

SECTIONS
{
	/* start address of the kernel image in VA */
	. = LOAD_OFFSET + CONFIG_PHYSICAL_START;
	/* get the physical address of the entry point */
	phys_kernel_entry = kernel_entry - LOAD_OFFSET;

	/*
	 * boot code
	 */
	.head.text : AT(ADDR(.head.text) - LOAD_OFFSET) {
		_text = .;
		HEAD_TEXT
		IDMAP_TEXT
	}

	/*
	 * text segment
	 */
	.text : AT(ADDR(.text) - LOAD_OFFSET) {
		_stext = .;
		TEXT_TEXT
		/* TODO: check if we really need what's below */
		SCHED_TEXT
		LOCK_TEXT
		KPROBES_TEXT
		ENTRY_TEXT
		IRQENTRY_TEXT
		*(.fixup)
		*(.gnu.warning)
		_etext = .;
	}

	EXCEPTION_TABLE(16)
	NOTES

	/*
	 * readonly/writable data segment
	 */
	_sdata = .;
	RO_DATA_SECTION(PAGE_SIZE)
	RW_DATA_SECTION(L1_CACHE_BYTES, PAGE_SIZE, THREAD_SIZE)

	/* TODO: do we need the following? */
	.lit8 : AT(ADDR(.lit8) - LOAD_OFFSET) { *(.lit8) }
	.lit4 : AT(ADDR(.lit4) - LOAD_OFFSET) { *(.lit4) }
	.sdata : AT(ADDR(.sdata) - LOAD_OFFSET) { *(.sdata) }
	_edata = .;

	/*
	 * init segment
	 * (will be freed after init, hence the page alignment)
	 *
	 * TODO: we could put this segment after bss, since it will be freed.
	 * Note: apparently it was ordered like that to avoid having several
	 * PT_LOAD segments (because of old bootloaders)
	 */
	. = ALIGN(PAGE_SIZE);
	.init.begin : AT(ADDR(.init.begin) - LOAD_OFFSET) {
		__init_begin = .;
	}
	INIT_TEXT_SECTION(PAGE_SIZE)
	INIT_DATA_SECTION(16)

	/* TODO: I don't fully understand what those are... */
	.exit.text : AT(ADDR(.exit.text) - LOAD_OFFSET) { EXIT_TEXT }
	.exit.data : AT(ADDR(.exit.data) - LOAD_OFFSET) { EXIT_DATA }

	PERCPU_SECTION(L1_CACHE_BYTES)

	/* align to PAGE_SIZE*2 (=8KiB):
	 * 1/ no holes before .bss (__init is supposed to be freed)
	 * 2/ .bss..swapper_pg_dir is expected to be PAGE_SIZE*2 aligned */
	. = ALIGN(PAGE_SIZE * 2);
	.init.end : AT(ADDR(.init.end) - LOAD_OFFSET) {
		__init_end = .;
	}

	/*
	 * bss segment
	 * - force .bss to PAGE_SIZE*2 (=8KiB) alignment
	 * - .sbss is supposed to be empty
	 */
	BSS_SECTION(0, PAGE_SIZE * 2, 0)
	_end = .;

	/*
	 * debug stuff
	 */
	STABS_DEBUG
	DWARF_DEBUG

	/* TODO: following necessary? */
	/* These must appear regardless of  .  */
	.gptab.sdata : {
		*(.gptab.data)
		*(.gptab.sdata)
	}
	.gptab.sbss : {
		*(.gptab.bss)
		*(.gptab.sbss)
	}

	DISCARDS
	/* TODO: isn't the following redundant with the sections we already
	 * remove in boot/Makefile? */
	/DISCARD/ : {
		/* ABI crap starts here */
		*(.MIPS.options)
		*(.options)
		*(.pdr)
		*(.reginfo)
	}
}
