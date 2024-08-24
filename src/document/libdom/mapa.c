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
	struct el_mapa *mapa = (struct el_mapa *)m;

	if (!mapa) {
		return;
	}

	if (mapa->size == mapa->allocated) {
		struct el_node_elem *tmp = mem_realloc(mapa->table, mapa->size * 2 * sizeof(*tmp));

		if (!tmp) {
			return;
		}
		mapa->table = tmp;
		mapa->allocated = mapa->size * 2;
	}

	mapa->table[mapa->size].offset = length;
	mapa->table[mapa->size].node = node;
	mapa->size++;
}

void
save_in_map2(void *m, void *node, int length)
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
		char *key = memacpy((const char *)&node, sizeof(node));

		if (key) {
			add_hash_item(mapa, key, sizeof(node), (void *)(intptr_t)offset);
		}
	}
}

void *
create_new_element_map(void)
{
	struct el_mapa *mapa = (struct el_mapa *)mem_calloc(1, sizeof(*mapa));

	if (!mapa) {
		return NULL;
	}
	struct el_node_elem *tmp = mem_calloc(256, sizeof(*tmp));

	if (!tmp) {
		mem_free(mapa);
		return NULL;
	}
	mapa->table = tmp;
	mapa->size = 0;
	mapa->allocated = 256;

	return mapa;
}

void *
create_new_element_map2(void)
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
	struct el_mapa *mapa = (struct el_mapa *)m;

	if (mapa) {
		mem_free(mapa->table);
		mem_free(mapa);
	}
}

void
delete_map2(void *m)
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
	delete_map2(m);
}

static int
compare(const void *a, const void *b)
{
	int offa = ((struct el_node_elem *)a)->offset;
	int offb = ((struct el_node_elem *)b)->offset;

	return offa - offb;
}

void *
find_in_map(void *m, int offset)
{
	struct el_mapa *mapa = (struct el_mapa *)m;

	if (!mapa) {
		return;
	}
	struct el_node_elem key = { .offset = offset, .node = NULL };
	struct el_node_elem *item = (struct el_node_elem *)bsearch(&key, mapa->table, mapa->size, sizeof(*item), compare);

	if (!item) {
		return NULL;
	}
	return item->node;
}

void *
find_in_map2(void *m, int offset)
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
	key = memacpy((const char *)&node, sizeof(node));

	if (!key) {
		return -1;
	}
	item = get_hash_item(mapa, key, sizeof(node));
	mem_free(key);

	if (!item) {
		return -1;
	}

	return (int)(intptr_t)(item->value);
}
