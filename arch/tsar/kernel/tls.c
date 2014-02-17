/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2013 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

#include <linux/syscalls.h>

#include <asm/mips32c0.h>
#include <asm/thread_info.h>

SYSCALL_DEFINE1(set_thread_area, unsigned long, addr)
{
	/* store the new tls address in kernel structure (so it can be restored
	 * when switching) */
	current_thread_info()->tp_value = addr;

	/* and also modify the cp0 register so the userland can now access it
	 * directly */
	write_c0_userlocal(addr);

	return 0;
}
