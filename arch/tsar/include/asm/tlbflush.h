#ifndef _ASM_TSAR_TLBFLUSH_H
#define _ASM_TSAR_TLBFLUSH_H

/*
 * Mandatory tlb flushing functions
 *
 * Nothing to do for us, TSAR is always coherent
 */

#define flush_tlb_mm(mm)			do { } while(0)
#define flush_tlb_page(vma, uaddr)		do { } while(0)
#define flush_tlb_range(vma, start, end)	do { } while(0)
#define flush_tlb_kernel_range(start, end)	do { } while(0)

#if 0
extern void flush_tlb_all(void);
extern void flush_tlb_kernel_page(unsigned long kaddr);
#endif

#endif /* _ASM_TSAR_TLBFLUSH_H */
