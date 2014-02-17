#ifndef _ASM_TSAR_ELF_H
#define _ASM_TSAR_ELF_H

#include <asm/ptrace.h>

/* ELF register definitions */
typedef unsigned long elf_greg_t;
#define ELF_NGREG (sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef double elf_fpreg_t;
#define ELF_NFPREG	33
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

/* ELF header e_flags defines. */
/* MIPS architecture level. */
#define EF_MIPS_ARCH_1		0x00000000	/* -mips1 code.	 */
#define EF_MIPS_ARCH_2		0x10000000	/* -mips2 code.	 */
#define EF_MIPS_ARCH_3		0x20000000	/* -mips3 code.	 */
#define EF_MIPS_ARCH_4		0x30000000	/* -mips4 code.	 */
#define EF_MIPS_ARCH_5		0x40000000	/* -mips5 code.	 */
#define EF_MIPS_ARCH_32		0x50000000	/* MIPS32 code.	 */
#define EF_MIPS_ARCH_64		0x60000000	/* MIPS64 code.	 */
#define EF_MIPS_ARCH_32R2	0x70000000	/* MIPS32 R2 code.  */
#define EF_MIPS_ARCH_64R2	0x80000000	/* MIPS64 R2 code.  */

/* The ABI of a file. */
#define EF_MIPS_ABI_O32		0x00001000	/* O32 ABI.  */
#define EF_MIPS_ABI_O64		0x00002000	/* O32 extended for 64 bit.  */

/* Flags in the e_flags field of the header */
#define EF_MIPS_NOREORDER	0x00000001
#define EF_MIPS_PIC		0x00000002
#define EF_MIPS_CPIC		0x00000004
#define EF_MIPS_ABI2		0x00000020
#define EF_MIPS_OPTIONS_FIRST	0x00000080
#define EF_MIPS_32BITMODE	0x00000100
#define EF_MIPS_ABI		0x0000f000
#define EF_MIPS_ARCH		0xf0000000

#define R_MIPS_NONE		0
#define R_MIPS_16		1
#define R_MIPS_32		2
#define R_MIPS_REL32		3
#define R_MIPS_26		4
#define R_MIPS_HI16		5
#define R_MIPS_LO16		6
#define R_MIPS_GPREL16		7
#define R_MIPS_LITERAL		8
#define R_MIPS_GOT16		9
#define R_MIPS_PC16		10
#define R_MIPS_CALL16		11
#define R_MIPS_GPREL32		12
/* The remaining relocs are defined on Irix, although they are not
   in the MIPS ELF ABI.	 */
#define R_MIPS_UNUSED1		13
#define R_MIPS_UNUSED2		14
#define R_MIPS_UNUSED3		15
#define R_MIPS_SHIFT5		16
#define R_MIPS_SHIFT6		17
#define R_MIPS_64		18
#define R_MIPS_GOT_DISP		19
#define R_MIPS_GOT_PAGE		20
#define R_MIPS_GOT_OFST		21
/*
 * The following two relocation types are specified in the MIPS ABI
 * conformance guide version 1.2 but not yet in the psABI.
 */
#define R_MIPS_GOTHI16		22
#define R_MIPS_GOTLO16		23
#define R_MIPS_SUB		24
#define R_MIPS_INSERT_A		25
#define R_MIPS_INSERT_B		26
#define R_MIPS_DELETE		27
#define R_MIPS_HIGHER		28
#define R_MIPS_HIGHEST		29

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(hdr)						\
({									\
	int __res = 1;							\
	struct elfhdr *__h = (hdr);					\
									\
	if (__h->e_machine != EM_MIPS)					\
		__res = 0;						\
	if (__h->e_ident[EI_CLASS] != ELFCLASS32)			\
		__res = 0;						\
	if ((__h->e_flags & EF_MIPS_ABI2) != 0)				\
		__res = 0;						\
	if (((__h->e_flags & EF_MIPS_ABI) != 0) &&			\
	    ((__h->e_flags & EF_MIPS_ABI) != EF_MIPS_ABI_O32))		\
		__res = 0;						\
									\
	__res;								\
})

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_MIPS

struct pt_regs;
struct task_struct;

extern void elf_dump_regs(elf_greg_t *, struct pt_regs *regs);
extern int dump_task_regs(struct task_struct *, elf_gregset_t *);
extern int dump_task_fpu(struct task_struct *, elf_fpregset_t *);

#ifndef ELF_CORE_COPY_REGS
#define ELF_CORE_COPY_REGS(elf_regs, regs)			\
	elf_dump_regs((elf_greg_t *)&(elf_regs), regs);
#endif
#ifndef ELF_CORE_COPY_TASK_REGS
#define ELF_CORE_COPY_TASK_REGS(tsk, elf_regs) dump_task_regs(tsk, elf_regs)
#endif
#define ELF_CORE_COPY_FPREGS(tsk, elf_fpregs)			\
	dump_task_fpu(tsk, elf_fpregs)

#define ELF_EXEC_PAGESIZE	PAGE_SIZE

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  This could be done in userspace,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	(0)

/*
 * This yields a string that ld.so will use to load implementation
 * specific libraries for optimization.	 This is more specific in
 * intent than poking at uname or /proc/cpuinfo.
 */

#define ELF_PLATFORM  (NULL) /* no specific optimizations with TSAR */

/*
 * See comments in asm-alpha/elf.h, this is the same thing
 * on the MIPS.
 */
#define ELF_PLAT_INIT(_r, load_addr)	do { \
	_r->regs[1] = _r->regs[2] = _r->regs[3] = _r->regs[4] = 0;	\
	_r->regs[5] = _r->regs[6] = _r->regs[7] = _r->regs[8] = 0;	\
	_r->regs[9] = _r->regs[10] = _r->regs[11] = _r->regs[12] = 0;	\
	_r->regs[13] = _r->regs[14] = _r->regs[15] = _r->regs[16] = 0;	\
	_r->regs[17] = _r->regs[18] = _r->regs[19] = _r->regs[20] = 0;	\
	_r->regs[21] = _r->regs[22] = _r->regs[23] = _r->regs[24] = 0;	\
	_r->regs[25] = _r->regs[26] = _r->regs[27] = _r->regs[28] = 0;	\
	_r->regs[30] = _r->regs[31] = 0;				\
} while (0)

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.	We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.	*/

#ifndef ELF_ET_DYN_BASE
#define ELF_ET_DYN_BASE		(TASK_SIZE / 3 * 2)
#endif

#endif /* _ASM_TSAR_ELF_H */
