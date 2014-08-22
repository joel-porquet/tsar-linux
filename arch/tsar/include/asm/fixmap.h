#ifndef _ASM_TSAR_FIXMAP_H
#define _ASM_TSAR_FIXMAP_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/page.h>

/* Leave an empty page between FIXADDR_TOP and the end of the virtual address
 * space. For safety I guess */
#define FIXADDR_TOP ((unsigned long)(-PAGE_SIZE))

enum fixed_addresses {
	__end_of_fixed_addresses
};

#define FIXADDR_SIZE ((unsigned long)__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START (FIXADDR_TOP - FIXADDR_SIZE)

extern void __set_fixmap(enum fixed_addresses idx,
		phys_addr_t paddr, pgprot_t prot);
#define __clear_fixmap(idx) __set_fixmap(idx, 0, __pgprot(0))

#define __fix_to_virt(x) (FIXADDR_TOP - ((x) << PAGE_SHIFT))
#define __virt_to_fix(x) ((FIXADDR_TOP - ((x) & PAGE_MASK)) >> PAGE_SHIFT)

/* This function doesn't actually exist, but the branch below should be
 * optimized everywhere so the function should never be called and thus get
 * discarded */
extern void __this_fixmap_does_not_exist(void);

static inline unsigned long fix_to_virt(const unsigned int idx)
{
	if (idx >= __end_of_fixed_addresses)
		__this_fixmap_does_not_exist();
	return __fix_to_virt(idx);
}

static inline unsigned long virt_to_fix(const unsigned long vaddr)
{
	BUG_ON(vaddr >= FIXADDR_TOP || vaddr < FIXADDR_START);
	return __virt_to_fix(vaddr);
}

#endif /* __ASSEMBLY__ */
#endif /* _ASM_TSAR_FIXMAP_H */
