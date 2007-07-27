#ifndef EL__UTIL_HASH_H
#define EL__UTIL_HASH_H

#include "util/lists.h"

/** This should be hopefully always 32bit at least. I'm not sure what will
 * happen when this will be of other length, but it should still work ok.
 * --pasky */
typedef unsigned long hash_value_T;
typedef hash_value_T (* hash_func_T)(unsigned char *key, unsigned int keylen, hash_value_T magic);

struct hash_item {
	LIST_HEAD(struct hash_item);

	unsigned char *key;
	unsigned int keylen;
	void *value;
};

struct hash {
	unsigned int width; /**< Number of bits - hash array must be 2^width long. */
	hash_func_T func;
	struct list_head hash[1]; /**< Must be at end ! */
};

struct hash *init_hash8(void);

void free_hash(struct hash **hashp);

struct hash_item *add_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen, void *value);
struct hash_item *get_hash_item(struct hash *hash, unsigned char *key, unsigned int keylen);
void del_hash_item(struct hash *hash, struct hash_item *item);

/** @relates hash */
#define foreach_hash_item(item, hash_table, iterator) \
	for (iterator = 0; iterator < (1 << (hash_table).width); iterator++) \
		foreach (item, (hash_table).hash[iterator])

#endif
