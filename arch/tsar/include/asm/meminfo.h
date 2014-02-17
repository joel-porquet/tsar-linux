#ifndef _ASM_MEMINFO_H
#define _ASM_MEMINFO_H

#include <linux/types.h>

#define NR_MEM_BANKS 8

struct membank {
	phys_addr_t start;
	phys_addr_t size;
	unsigned int highmem;
};

struct meminfo {
	int nr_banks;
	struct membank bank[NR_MEM_BANKS];
};

extern struct meminfo meminfo;

#define for_each_membank(iter, mi)			\
	for (iter = 0; iter < (mi)->nr_banks; iter++)

#define membank_phys_start(bank)	((bank)->start)
#define membank_phys_end(bank)		((bank)->start + (bank)->size)
#define membank_phys_size(bank)		((bank)->size)

extern int meminfo_add_membank (phys_addr_t start, phys_addr_t size);

#endif /* _ASM_MEMINFO_H */
