/* map temporary file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "elinks.h"

#include "document/libdom/mapa.h"
#include "util/hash.h"
#include "util/string.h"

void
save_in_map(void *m, void *node, int length)
{
	struct hash *mapa = (struct hash *)m;

	if (mapa) {
		char *key = memacpy((const char *)&length, sizeof(length));

		if (key) {
			add_hash_item(mapa, key, sizeof(length), node);
		}
	}
}

void
save_offset_in_map(void *m, void *node, int offset)
{
	struct hash *mapa = (struct hash *)m;

	if (mapa) {
		char *key = memacpy((const char *)node, sizeof(node));

		if (key) {
			add_hash_item(mapa, key, sizeof(node), (void *)offset);
		}
	}
}

void *
create_new_element_map(void)
{
	return (void *)init_hash8();
}

void *
create_new_element_map_rev(void)
{
	return (void *)init_hash8();
}

void
delete_map(void *m)
{
	struct hash *hash = (struct hash *)(*(struct hash **)m);
	struct hash_item *item;
	int i;

	foreach_hash_item(item, *hash, i) {
		mem_free_set(&item->key, NULL);
	}
	free_hash(m);
}

void
delete_map_rev(void *m)
{
	delete_map(m);
}

void *
find_in_map(void *m, int offset)
{
	struct hash *mapa = (struct hash *)m;
	struct hash_item *item;
	char *key;

	if (!mapa) {
		return NULL;
	}
	key = memacpy((const char *)&offset, sizeof(offset));

	if (!key) {
		return NULL;
	}
	item = get_hash_item(mapa, key, sizeof(offset));
	mem_free(key);

	if (!item) {
		return NULL;
	}

	return item->value;
}

int
find_offset(void *m, void *node)
{
	struct hash *mapa = (struct hash *)m;
	struct hash_item *item;
	char *key;

	if (!mapa) {
		return -1;
	}
	key = memacpy((const char *)node, sizeof(node));

	if (!key) {
		return -1;
	}
	item = get_hash_item(mapa, key, sizeof(node));
	mem_free(key);

	if (!item) {
		return -1;
	}

	return (int)(item->value);
}
