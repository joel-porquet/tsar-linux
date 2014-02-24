/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2014 Pierre and Marie Curie University
 *  Joël Porquet <joel.porquet@lip6.fr>
 */

/* Heavily inspired by openrisc implementation */

#include <linux/linkage.h>

#include <asm/regdef.h>

/*
 * long __copy_tofrom_user(void *to, const void __user * from, unsigned long n);
 *
 * returns 0 on success or the number of bytes NOT copied in case of error
 */
ENTRY(__copy_tofrom_user)
	.set push
	.set noat
	.set noreorder
	add	v0, a2, zero
2:
	beqz	v0, 1f
	addiu	v0, -1
8:	lbu	t0, 0(a1)
9:	sb	t0, 0(a0)
	addiu	a1, 1
	j	2b
	addiu	a0, 1
1:
	jr	ra
	addiu	v0, 1
	.set pop

	.section .fixup, "ax"
99:	j	1b
	.previous
	.section __ex_table, "a"

	.long 8b, 99b
	.long 9b, 99b
	.previous
ENDPROC(__copy_tofrom_user)

/*
 * unsigned long __clear_user(void __user *to, unsigned long n);
 *
 * returns 0 on success or the number of bytes NOT copied in case of error
 */
ENTRY(__clear_user)
	.set push
	.set noat
	.set noreorder
	add	v0, a1, zero
2:
	beqz	v0, 1f
	addiu	v0, -1
9:	sb	zero, 0(a0)
	j	2b
	addiu	a0, 1
1:
	jr	ra
	addiu	v0, 1
	.set pop

	.section .fixup, "ax"
99:	j	1b
	.previous

	.section __ex_table, "a"
	.long 9b, 99b
	.previous
ENDPROC(__clear_user)
