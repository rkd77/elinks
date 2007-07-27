#ifndef EL__UTIL_BITFIELD_H
#define EL__UTIL_BITFIELD_H

#include <string.h>

#include "util/memory.h"

/* Bitfield operations: */

/** A vector of bits.  The size is fixed at initialization time.  */
struct bitfield {
	unsigned int bitsize;	/**< Number of bits in the bitfield. */
	unsigned char bits[1];	/**< Strawberry bitfields forever. */
};

/** @relates bitfield */
#define foreach_bitfield_set(bit, bitfield) \
	for ((bit) = 0; (bit) < (bitfield)->bitsize; (bit)++) \
		if (test_bitfield_bit(bitfield, bit))

/** @relates bitfield */
#define foreachback_bitfield_set(bit, bitfield) \
	for ((bit) = (bitfield)->bitsize; (bit) > 0;) \
		if (test_bitfield_bit(bitfield, --bit))

/** @relates bitfield */
#define foreach_bitfield_cleared(bit, bitfield) \
	for ((bit) = 0; (bit) < (bitfield)->bitsize; (bit)++) \
		if (!test_bitfield_bit(bitfield, bit))

/* By shifting when getting the bit offset it is ensured that only one bit is
 * set in the resulting value. Idea from libbt by Kevin Smathers. */
#define get_bitfield_bit_offset(bit)	((size_t) (0x80 >> ((bit) % 8)))
#define get_bitfield_byte_offset(bit)	((size_t) ((bit) / 8))
/* +7 to round up to nearest byte. */
#define get_bitfield_byte_size(bits)	((size_t) (((bits) + 7) / 8))

/** Allocate a bitfield containing @a bits number of bits.
 * @relates bitfield */
static inline struct bitfield *
init_bitfield(size_t bits)
{
	size_t size = get_bitfield_byte_size(bits);
	struct bitfield *bitfield;

	bitfield = mem_calloc(1, offsetof(struct bitfield, bits) + size);
	if (bitfield) bitfield->bitsize = bits;

	return bitfield;
}

/** Update @a bitfield with the @a bytesize bytes from the bit string
 * in @a bits.
 * @relates bitfield */
static inline void
copy_bitfield(struct bitfield *bitfield,
	      const unsigned char *bits, unsigned int bytesize)
{
	/* Only for exact size? */
	if (bytesize <= get_bitfield_byte_size(bitfield->bitsize))
		memcpy(bitfield->bits, bits, bytesize);
}

/** Test whether @a bit is set in the @a bitfield.
 * @relates bitfield */
static inline int
test_bitfield_bit(struct bitfield *bitfield, unsigned int bit)
{
	size_t byte_offset, bit_offset;

	if (bit >= bitfield->bitsize)
		return 0;

	byte_offset = get_bitfield_byte_offset(bit);
	bit_offset  = get_bitfield_bit_offset(bit);

	return !!(bitfield->bits[byte_offset] & bit_offset);
}

/** Set @a bit in the @a bitfield.
 * @relates bitfield */
static inline void
set_bitfield_bit(struct bitfield *bitfield, unsigned int bit)
{
	size_t byte_offset, bit_offset;

	if (bit >= bitfield->bitsize)
		return;

	byte_offset = get_bitfield_byte_offset(bit);
	bit_offset  = get_bitfield_bit_offset(bit);

	bitfield->bits[byte_offset] |= bit_offset;
}

/** Unset @a bit in the @a bitfield.
 * @relates bitfield */
static inline void
clear_bitfield_bit(struct bitfield *bitfield, unsigned int bit)
{
	size_t byte_offset, bit_offset;

	if (bit >= bitfield->bitsize)
		return;

	byte_offset = get_bitfield_byte_offset(bit);
	bit_offset  = get_bitfield_bit_offset(bit);

	bitfield->bits[byte_offset] &= ~bit_offset;
}

/** Count the set bits in @a bitfield.
 * @relates bitfield */
static inline unsigned int
get_bitfield_set_count(struct bitfield *bitfield)
{
	unsigned int bit, count = 0;

	foreach_bitfield_set (bit, bitfield)
		count++;

	return count;
}

/** Count the unset bits in @a bitfield.
 * @relates bitfield */
static inline unsigned int
get_bitfield_cleared_count(struct bitfield *bitfield)
{
	unsigned int bit, count = 0;

	foreach_bitfield_cleared (bit, bitfield)
		count++;

	return count;
}

/** Check whether all bits of @a bitfield are set.
 * @relates bitfield */
static inline unsigned int
bitfield_is_set(struct bitfield *bitfield)
{
	unsigned int bit;

	foreach_bitfield_cleared (bit, bitfield)
		return 0;

	return 1;
}

/** Check whether all bits of @a bitfield are unset.
 * @relates bitfield */
static inline unsigned int
bitfield_is_cleared(struct bitfield *bitfield)
{
	unsigned int bit;

	foreach_bitfield_set (bit, bitfield)
		return 0;

	return 1;
}

#endif
