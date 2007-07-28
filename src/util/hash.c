/** Hashing infrastructure
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "util/hash.h"
#include "util/memory.h"


/* String hashes functions chooser. */
/* #define MIX_HASH */ /* Far more better but slower */
#define X31_HASH /* Weaker but faster */


/* We provide common infrastructure for hashing - each hash consists from one
 * particularly large array full of small lists of keys with same index in the
 * array (same hash value). */

#define hash_mask(n) (hash_size(n) - 1)
#define hash_size(n) (1 << (n))

static hash_value_T strhash(unsigned char *k, unsigned int length, hash_value_T initval);

static inline struct hash *
init_hash(unsigned int width, hash_func_T func)
{
	struct hash *hash;
	unsigned int i = 0;

	assert(width > 0 && func);
	if_assert_failed return NULL;

	/* One is already reserved in struct hash, so use size - 1. */
	hash = mem_alloc(sizeof(*hash) + (hash_size(width) - 1)
			 * sizeof(struct list_head));
	if (!hash) return NULL;

	hash->width = width;
	hash->func = func;

	/* Initialize dummy list_heads */
	for (; i < hash_size(width); i++)
		init_list(hash->hash[i]);

	return hash;
}

/** @relates hash */
struct hash *
init_hash8(void)
{
	return init_hash(8, &strhash);
}

/** @relates hash */
void
free_hash(struct hash **hashp)
{
	unsigned int i = 0;

	assert(hashp && *hashp);
	if_assert_failed return;

	for (; i < hash_size((*hashp)->width); i++)
		free_list((*hashp)->hash[i]);

	mem_free_set(hashp, NULL);
}


/** Initialization vector for the hash function.
 * I've no much idea about what to set here.. I think it doesn't matter much
 * anyway.. ;) --pasky */
#define HASH_MAGIC 0xdeadbeef

/** @returns hash_item if ok, NULL if error.
 * @relates hash */
struct hash_item *
add_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen,
	      void *value)
{
	hash_value_T hashval;
	struct hash_item *item = mem_alloc(sizeof(*item));

	if (!item) return NULL;

	hashval = hash->func(key, keylen, HASH_MAGIC) & hash_mask(hash->width);

	item->key = key;
	item->keylen = keylen;
	item->value = value;

	add_to_list(hash->hash[hashval], item);

	return item;
}

/** @relates hash */
struct hash_item *
get_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen)
{
	struct list_head *list;
	struct hash_item *item;
	hash_value_T hashval;

	hashval = hash->func(key, keylen, HASH_MAGIC) & hash_mask(hash->width);
	list    = &hash->hash[hashval];

	foreach (item, *list) {
		if (keylen != item->keylen) continue;
		if (memcmp(key, item->key, keylen)) continue;

		/* Watch the MFR (Move Front Rule)! Basically, it self-orders
		 * the list by popularity of its items. Inspired from Links,
		 * probably PerM. --pasky */
		move_to_top_of_list(*list, item);

		return item;
	}

	return NULL;
}

#undef HASH_MAGIC

/** Delete @a item from @a hash.
 * If key and/or value were dynamically allocated, think about freeing them.
 * This function doesn't do that.
 * @relates hash */
void
del_hash_item(struct hash *hash, struct hash_item *item)
{
	assert(item);
	if_assert_failed return;

	del_from_list(item);
	mem_free(item);
}


#ifdef X31_HASH

/** Fast string hashing.
 * @param k		the key
 * @param length	the length of the key
 * @param initval	the previous hash, or an arbitrary value */
static hash_value_T
strhash(unsigned char *k,
	unsigned int length,
	hash_value_T initval)
{
	const unsigned char *p = (const unsigned char *) k;
	hash_value_T h = initval;
	unsigned int i = 0;

	assert(k && length > 0);
	if_assert_failed return h;

	/* This loop was unrolled, should improve performance on most cpus,
	 * After some tests, 8 seems a good value for most keys. --Zas */
	for (;;) {
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
		h = (h << 5) - h + p[i];
		if (++i == length) break;
	};

	return h;
}

#else /* X31_HASH */

/* String hashing function follows; it is not written by me, somewhere below
 * are credits. I only hacked it a bit. --pasky */

/* This is a big CPU hog, in fact:
 *
 *   %   cumulative   self              self     total-----------
 *  time   seconds   seconds    calls  us/call  us/call  name----
 *   6.00      0.35     0.06    10126     5.93     5.93  strhash
 *
 * It should be investigated whether we couldn't push this down a little. */

/*
 *  --------------------------------------------------------------------
 *   mix -- mix 3 32-bit values reversibly.
 *   For every delta with one or two bits set, and the deltas of all three
 *     high bits or all three low bits, whether the original value of a,b,c
 *     is almost all zero or is uniformly distributed,
 *   * If mix() is run forward or backward, at least 32 bits in a,b,c
 *     have at least 1/4 probability of changing.
 *   * If mix() is run forward, every bit of c will change between 1/3 and
 *     2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 *     mix() was built out of 36 single-cycle latency instructions in a
 *     structure that could supported 2x parallelism, like so:
 *       a -= b;
 *       a -= c; x = (c>>13);
 *       b -= c; a ^= x;
 *       b -= a; x = (a<<8);
 *       c -= a; b ^= x;
 *       c -= b; x = (b>>13);
 *       ...
 *     Unfortunately, superscalar Pentiums and Sparcs can't take advantage
 *     of that parallelism.  They've also turned some of those single-cycle
 *     latency instructions into multi-cycle latency instructions.  Still,
 *     this is the fastest good hash I could find.  There were about 2^^68
 *     to choose from.  I only looked at a billion or so.
 *  --------------------------------------------------------------------
*/

#define mix(a, b, c) { \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<<8); \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12);  \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>>5); \
	a -= b; a -= c; a ^= (c>>3);  \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
}

 /*
 --------------------------------------------------------------------
 hash() -- hash a variable-length key into a 32-bit value
   k       : the key (the unaligned variable-length array of bytes)
   len     : the length of the key, counting by bytes
   initval : can be any 4-byte value
 Returns a 32-bit value.  Every bit of the key affects every bit of
 the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 About 6*len+35 instructions.

 The best hash table sizes are powers of 2.  There is no need to do
 mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 use a bitmask.  For example, if you need only 10 bits, do
   h = (h & hashmask(10));
 In which case, the hash table should have hashsize(10) elements.

 If you are hashing n strings (ub1 **) k, do it like this:
   for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

 By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
 code any way you wish, private, educational, or commercial.  It's free.

 See http://burtleburtle.net/bob/hash/evahash.html
 Use for hash table lookup, or anything where one collision in 2^^32 is
 acceptable.  Do NOT use for cryptographic purposes.
 --------------------------------------------------------------------
 */

#define keycompute(a) ((k[(a)]) \
			+ ((hash_value_T) (k[(a)+1])<<8) \
			+ ((hash_value_T) (k[(a)+2])<<16) \
			+ ((hash_value_T) (k[(a)+3])<<24))

/** Hash an array of bytes.
 * @param k		the key
 * @param length	the length of the key
 * @param initval	the previous hash, or an arbitrary value */
static hash_value_T
strhash(unsigned char *k,
	unsigned int length,
	hash_value_T initval)
{
	int len;
	hash_value_T a, b, c;

	/* Set up the internal state */
	len = length;
	a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
	c = initval;         /* the previous hash value */

	/*---------------------------------------- handle most of the key */
	while (len >= 12) {
		a += keycompute(0);
		b += keycompute(4);
		c += keycompute(8);
		mix(a, b, c);
		k += 12;
		len -= 12;
	}

	/*------------------------------------- handle the last 11 bytes */
	c += length;
	switch (len) {	/* all the case statements fall through */
		case 11: c += ((hash_value_T) (k[10])<<24);
		case 10: c += ((hash_value_T) (k[9])<<16);
		case 9 : c += ((hash_value_T) (k[8])<<8);
			/* the first byte of c is reserved for the length */
		case 8 : b += ((hash_value_T) (k[7])<<24);
		case 7 : b += ((hash_value_T) (k[6])<<16);
		case 6 : b += ((hash_value_T) (k[5])<<8);
		case 5 : b += (k[4]);
		case 4 : a += ((hash_value_T) (k[3])<<24);
		case 3 : a += ((hash_value_T) (k[2])<<16);
		case 2 : a += ((hash_value_T) (k[1])<<8);
		case 1 : a += (k[0]);
			/* case 0: nothing left to add */
	}

	mix(a, b, c);

	/*-------------------------------------------- report the result */
	return c;
}

#undef keycompute
#undef mix
#undef hash_mask

#endif /* MIX_HASH */
