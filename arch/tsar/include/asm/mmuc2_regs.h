#ifndef _ASM_TSAR_MMUC2_REGS_H
#define _ASM_TSAR_MMUC2_REGS_H

/*
 * Coprocessor 2 (MMU) register names
 */
#define MMU_PTPR 		$0 	/* (RW) Page Table Pointer Register */
#define MMU_MODE 		$1 	/* (RW) Mode Register */
#define MMU_ICACHE_FLUSH 	$2 	/* (W)  Instruction Cache flush */
#define MMU_DCACHE_FLUSH 	$3 	/* (W)  Data Cache flush */
#define MMU_ITLB_INVAL 		$4 	/* (W)  Instruction TLB line invalidation */
#define MMU_DTLB_INVAL 		$5 	/* (W)  Data TLB line Invalidation */
#define MMU_ICACHE_INVAL 	$6 	/* (W)  Instruction Cache line invalidation */
#define MMU_DCACHE_INVAL 	$7 	/* (W)  Data Cache line invalidation */
#define MMU_ICACHE_PREFETCH 	$8 	/* (W)  Instruction Cache line prefetch */
#define MMU_DCACHE_PREFETCH 	$9 	/* (W)  Data Cache line prefetch */
#define MMU_SYNC 		$10 	/* (W)  Complete pending writes (note
					   that MIPS provides us with a sync
					   inst that cause the same thing) */
#define MMU_IETR 		$11 	/* (R)  Instruction Exception Type Register */
#define MMU_DETR 		$12 	/* (R)  Data Exception Type Register */
#define MMU_IBVAR 		$13 	/* (R)  Instruction Bad Virtual Address Register */
#define MMU_DBVAR 		$14 	/* (R)  Data Bad Virtual Address Register */
#define MMU_PARAMS 		$15 	/* (R)  Caches & TLBs hardware parameters */
#define MMU_RELEASE 		$16 	/* (R)  Generic MMU release number */
#define MMU_WORD_LO 		$17 	/* (RW) Lowest part of a double word */
#define MMU_WORD_HI 		$18 	/* (RW) Highest part of a double word */
#define MMU_ICACHE_PA_INVAL 	$19 	/* (W)  Instruction cache inval physical address */
#define MMU_DCACHE_PA_INVAL 	$20 	/* (W)  Data cache inval physical address */
#define MMU_LL_RESET		$21 	/* (W)  LLSC reservation buffer invalidation */

/*
 * Bitfields
 */
#define MMU_MODE_INST_TLB	0x8	/* instruction TLB and associated MMU hardware mechanism */
#define MMU_MODE_DATA_TLB	0x4	/* data TLB and associated MMU hardware mechanism */
#define MMU_MODE_INST_CACHE	0x2	/* instruction cache */
#define MMU_MODE_DATA_CACHE	0x1	/* data cache */

/* mmu_ietr and mmu_detr */
#define MMU_RW_MASK	0x1000	/* write or read faulty access */
#define MMU_ETR_WRITE	0x0000	/* the error concerns a write access (bit #12) */
#define MMU_ETR_READ	0x1000	/* the error concerns a read access (bit #12) */

#define MMU_ERR_MASK		0xFFF	/* error of faulty access */
#define MMU_PT1_UNMAPPED	0x001 	/* Page fault on Table 1 (invalid PTE)	(non fatal error) */
#define MMU_PT2_UNMAPPED	0x002 	/* Page fault on Table 2 (invalid PTE) 	(non fatal error) */
#define MMU_PRIVILEGE_VIOLATION	0x004 	/* Protected access in user mode 	(user error) */
#define MMU_ACCES_VIOLATION	0x008 	/* Write to a non writable page 	(user error) */
#define MMU_EXEC_VIOLATION	0x010 	/* Exec access to a non exec page 	(user error) */
#define MMU_UNDEFINED_XTN	0x020 	/* Undefined external access address 	(user error) */
#define MMU_PT1_ILLEGAL_ACCESS	0x040 	/* Bus Error in Table 1 access		(kernel error) */
#define MMU_PT2_ILLEGAL_ACCESS	0x080 	/* Bus Error in Table 2 access		(kernel error) */
#define MMU_DATA_ILLEGAL_ACCESS	0x100 	/* Bus Error during the cache access 	(kernel error) */

#endif /* _ASM_TSAR_MMUC2_REGS_H */
