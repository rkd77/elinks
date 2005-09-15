/* $Id: hash.h,v 1.8 2003/09/16 14:58:53 zas Exp $ */

#ifndef EL__UTIL_HASH_H
#define EL__UTIL_HASH_H

#include "util/lists.h"

/* This should be hopefully always 32bit at least. I'm not sure what will
 * happen when this will be of other length, but it should still work ok.
 * --pasky */
typedef unsigned long hash_value;
typedef hash_value (* hash_func)(unsigned char *key, unsigned int keylen, hash_value magic);

struct hash_item {
	LIST_HEAD(struct hash_item);

	unsigned char *key;
	unsigned int keylen;
	void *value;
};

struct hash {
	unsigned int width; /* Number of bits - hash array must be 2^width long. */
	hash_func func;
	struct list_head hash[1]; /* Must be at end ! */
};

#define hash_size(n) (1 << (n))

struct hash *init_hash(unsigned int width, hash_func func);
void free_hash(struct hash *hash);

struct hash_item *add_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen, void *value);
struct hash_item *get_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen);
void del_hash_item(struct hash *hash, struct hash_item *item);
hash_value strhash(unsigned char *k, unsigned int length, hash_value initval);

#define foreach_hash_item(item, hash_table, iterator) \
	for (iterator = 0; iterator < hash_size((hash_table).width); iterator++) \
		foreach (item, (hash_table).hash[iterator])

#endif
