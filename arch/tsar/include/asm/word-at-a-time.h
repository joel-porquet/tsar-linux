#ifndef _ASM_TSAR_WORD_AT_A_TIME_H
#define _ASM_TSAR_WORD_AT_A_TIME_H

#include <linux/kernel.h>

/*
 * Little-endian version for fast detection of zero bytes in word units.
 */

/*
 * Generate two magic words:
 * 0x01010101 and 0x80808080
 */

struct word_at_a_time {
	const unsigned long one_bits, high_bits;
};

#define WORD_AT_A_TIME_CONSTANTS { REPEAT_BYTE(0x01), REPEAT_BYTE(0x80) }


/*
 * Detect whether an unsigned long contains a zero byte
 * See http://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
 * for more explanation about it
 */
static inline unsigned long
has_zero(unsigned long a, unsigned long *bits, const struct word_at_a_time *c)
{
	unsigned long mask = ((a - c->one_bits) & ~a) & c->high_bits;
	*bits = mask;
	return mask;
}

#define prep_zero_mask(a, bits, c) (bits)

static inline unsigned long create_zero_mask(unsigned long bits)
{
	bits = (bits - 1) & ~bits;
	return bits >> 7;
}

static inline unsigned long find_zero(unsigned long mask)
{
	return fls(mask) >> 3;
}

#define zero_bytemask(mask) (mask)

#endif /* _ASM_TSAR_WORD_AT_A_TIME_H */
