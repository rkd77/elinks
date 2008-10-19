/* Blacklist manager */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/http/blacklist.h"
#include "protocol/uri.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


struct blacklist_entry {
	LIST_HEAD(struct blacklist_entry);

	enum blacklist_flags flags;
	unsigned char host[1]; /* Must be last. */
};

static INIT_LIST_OF(struct blacklist_entry, blacklist);


static struct blacklist_entry *
get_blacklist_entry(struct uri *uri)
{
	struct blacklist_entry *entry;

	assert(uri && uri->hostlen > 0);
	if_assert_failed return NULL;

	foreach (entry, blacklist)
		if (!c_strlcasecmp(entry->host, -1, uri->host, uri->hostlen))
			return entry;

	return NULL;
}

void
add_blacklist_entry(struct uri *uri, enum blacklist_flags flags)
{
	struct blacklist_entry *entry = get_blacklist_entry(uri);

	if (entry) {
		entry->flags |= flags;
		return;
	}

	entry = mem_alloc(sizeof(*entry) + uri->hostlen);
	if (!entry) return;

	entry->flags = flags;
	safe_strncpy(entry->host, uri->host, uri->hostlen + 1);
	add_to_list(blacklist, entry);
}

void
del_blacklist_entry(struct uri *uri, enum blacklist_flags flags)
{
	struct blacklist_entry *entry = get_blacklist_entry(uri);

	if (!entry) return;

	entry->flags &= ~flags;
	if (entry->flags) return;

	del_from_list(entry);
	mem_free(entry);
}

enum blacklist_flags
get_blacklist_flags(struct uri *uri)
{
	struct blacklist_entry *entry = get_blacklist_entry(uri);

	return entry ? entry->flags : SERVER_BLACKLIST_NONE;
}

void
free_blacklist(void)
{
	free_list(blacklist);
}
