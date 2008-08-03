/* Cache subsystem */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cache/dialogs.h"
#include "config/options.h"
#include "main/main.h"
#include "main/object.h"
#include "network/connection.h"
#include "protocol/protocol.h"
#include "protocol/proxy.h"
#include "protocol/uri.h"
#ifdef CONFIG_SCRIPTING_SPIDERMONKEY
# include "scripting/smjs/smjs.h"
#endif
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"

/* The list of cache entries */
static INIT_LIST_OF(struct cache_entry, cache_entries);

static unsigned longlong cache_size;
static int id_counter = 1;

static void truncate_entry(struct cache_entry *cached, off_t offset, int final);

/* Change 0 to 1 to enable cache debugging features (redirect stderr to a file). */
#if 0
#define DEBUG_CACHE
#endif

#ifdef DEBUG_CACHE

#define dump_frag(frag, count) \
do { \
	DBG(" [%d] f=%p offset=%" OFF_PRINT_FORMAT \
	    " length=%" OFF_PRINT_FORMAT \
	    " real_length=%" OFF_PRINT_FORMAT, \
	    count, frag, (off_print_T) frag->offset, \
	    (off_print_T) frag->length, (off_print_T) frag->real_length); \
} while (0)

#define dump_frags(entry, comment) \
do { \
	struct fragment *frag; \
        int count = 0;	\
 \
	DBG("%s: url=%s, cache_size=%li", comment, struri(entry->uri), cache_size); \
	foreach (frag, entry->frag) \
		dump_frag(frag, ++count); \
} while (0)

#else
#define dump_frags(entry, comment)
#endif /* DEBUG_CACHE */

unsigned longlong
get_cache_size(void)
{
	return cache_size;
}

int
get_cache_entry_count(void)
{
	return list_size(&cache_entries);
}

int
get_cache_entry_used_count(void)
{
	struct cache_entry *cached;
	int i = 0;

	foreach (cached, cache_entries)
		i += is_object_used(cached);

	return i;
}

int
get_cache_entry_loading_count(void)
{
	struct cache_entry *cached;
	int i = 0;

	foreach (cached, cache_entries)
		i += is_entry_used(cached);

	return i;
}

struct cache_entry *
find_in_cache(struct uri *uri)
{
	struct cache_entry *cached;
	int proxy = (uri->protocol == PROTOCOL_PROXY);

	foreach (cached, cache_entries) {
		struct uri *c_uri;

		if (!cached->valid) continue;

		c_uri = proxy ? cached->proxy_uri : cached->uri;
		if (!compare_uri(c_uri, uri, URI_BASE))
			continue;

		move_to_top_of_list(cache_entries, cached);

		return cached;
	}

	return NULL;
}

struct cache_entry *
get_cache_entry(struct uri *uri)
{
	struct cache_entry *cached = find_in_cache(uri);

	assertm(!uri->fragment, "Fragment in URI (%s)", struri(uri));

	if (cached) return cached;

	shrink_memory(0);

	cached = mem_calloc(1, sizeof(*cached));
	if (!cached) return NULL;

	cached->uri = get_proxied_uri(uri);
	if (!cached->uri) {
		mem_free(cached);
		return NULL;
	}

	cached->proxy_uri = get_proxy_uri(uri, NULL);
	if (!cached->proxy_uri) {
		done_uri(cached->uri);
		mem_free(cached);
		return NULL;
	}
	cached->incomplete = 1;
	cached->valid = 1;

	init_list(cached->frag);
	cached->cache_id = id_counter++;
	object_nolock(cached, "cache_entry"); /* Debugging purpose. */

	cached->box_item = add_listbox_leaf(&cache_browser, NULL, cached);

	add_to_list(cache_entries, cached);

	return cached;
}

static int
cache_entry_has_expired(struct cache_entry *cached)
{
	timeval_T now;

	timeval_now(&now);

	return timeval_cmp(&cached->max_age, &now) <= 0;
}

struct cache_entry *
get_validated_cache_entry(struct uri *uri, enum cache_mode cache_mode)
{
	struct cache_entry *cached;

	/* We have to check if something should be reloaded */
	if (cache_mode > CACHE_MODE_NORMAL)
		return NULL;

	/* We only consider complete entries */
	cached = find_in_cache(uri);
	if (!cached || cached->incomplete)
		return NULL;


	/* A bit of a gray zone. Delete the entry if the it has the strictest
	 * cache mode and we don't want the most aggressive mode or we have to
	 * remove the redirect or the entry expired. Please enlighten me.
	 * --jonas */
	if ((cached->cache_mode == CACHE_MODE_NEVER && cache_mode != CACHE_MODE_ALWAYS)
	    || (cached->redirect && !get_opt_bool("document.cache.cache_redirects"))
	    || (cached->expire && cache_entry_has_expired(cached))) {
		if (!is_object_used(cached)) delete_cache_entry(cached);
		return NULL;
	}

	if (cached->cache_mode <= CACHE_MODE_CHECK_IF_MODIFIED
	    && cache_mode <= CACHE_MODE_CHECK_IF_MODIFIED
	    && (cached->last_modified || cached->etag)
	    && get_opt_int("document.cache.revalidation_interval") >= 0) {
		if (cached->seconds + get_opt_int("document.cache.revalidation_interval") < time(NULL))
			return NULL;
	}

	return cached;
}

int
cache_entry_is_valid(struct cache_entry *cached)
{
	struct cache_entry *valid_cached;

	foreach (valid_cached, cache_entries) {
		if (valid_cached == cached)
			return 1;
	}

	return 0;
}


struct cache_entry *
follow_cached_redirects(struct cache_entry *cached)
{
	int redirects = 0;

	while (cached) {
		if (!cached->redirect) {
			/* XXX: This is not quite true, but does that difference
			 * matter here? */
			return cached;
		}

		if (++redirects > MAX_REDIRECTS) break;

		cached = find_in_cache(cached->redirect);
	}

	return NULL;
}

struct cache_entry *
get_redirected_cache_entry(struct uri *uri)
{
	struct cache_entry *cached = find_in_cache(uri);

	return cached ? follow_cached_redirects(cached) : NULL;
}


static inline void
enlarge_entry(struct cache_entry *cached, off_t size)
{
	cached->data_size += size;
	assertm(cached->data_size >= 0,
		"cache entry data_size underflow: %ld", cached->data_size);
	if_assert_failed { cached->data_size = 0; }

	cache_size += size;
	assertm(cache_size >= 0, "cache_size underflow: %ld", cache_size);
	if_assert_failed { cache_size = 0; }
}


#define CACHE_PAD(x) (((x) | 0x3fff) + 1)

/* One byte is reserved for data in struct fragment. */
#define FRAGSIZE(x) (sizeof(struct fragment) + (x) - 1)

/* We store the fragments themselves in a private vault, safely separated from
 * the rest of memory structures. If we lived in the main libc memory pool, we
 * would trigger annoying pathological behaviour like artificially enlarging
 * the memory pool to 50M, then securing it with some stupid cookie record at
 * the top and then no matter how you flush the cache the data segment is still
 * 50M big.
 *
 * Cool, but we don't want that, so fragments (where the big data is stored)
 * live in their little mmap()ed worlds. There is some overhead, but if we
 * assume single fragment per cache entry and page size (mmap() allocation
 * granularity) 4096, for a squad of ten 1kb documents this amounts 30kb.
 * That's not *that* horrible when you realize that the freshmeat front page
 * takes 300kb in memory and we usually do not deal with documents so small
 * that max. 4kb overhead would be visible there.
 *
 * The alternative would be of course to manage an entire custom memory pool,
 * but that is feasible only when we are able to resize it efficiently. We
 * aren't, except on Linux.
 *
 * Of course for all this to really completely prevent the pathological cases,
 * we need to stuff the rendered documents in too, because they seem to amount
 * the major memory bursts. */

static struct fragment *
frag_alloc(size_t size)
{
	struct fragment *f = mem_mmap_alloc(FRAGSIZE(size));

	if (!f) return NULL;
	memset(f, 0, FRAGSIZE(size));
	return f;
}

static struct fragment *
frag_realloc(struct fragment *f, size_t size)
{
	return mem_mmap_realloc(f, FRAGSIZE(f->real_length), FRAGSIZE(size));
}

static void
frag_free(struct fragment *f)
{
	mem_mmap_free(f, FRAGSIZE(f->real_length));
}


/* Concatenate overlapping fragments. */
static void
remove_overlaps(struct cache_entry *cached, struct fragment *f, int *trunc)
{
	off_t f_end_offset = f->offset + f->length;

	/* Iterate thru all fragments we still overlap to. */
	while (list_has_next(cached->frag, f)
		&& f_end_offset > f->next->offset) {
		struct fragment *nf;
		off_t end_offset = f->next->offset + f->next->length;

		if (f_end_offset < end_offset) {
			/* We end before end of the following fragment, though.
			 * So try to append overlapping part of that fragment
			 * to us. */
			nf = frag_realloc(f, end_offset - f->offset);
			if (nf) {
				nf->prev->next = nf;
				nf->next->prev = nf;
				f = nf;

				if (memcmp(f->data + f->next->offset - f->offset,
					   f->next->data,
					   f->offset + f->length - f->next->offset))
					*trunc = 1;

				memcpy(f->data + f->length,
				       f->next->data + f_end_offset - f->next->offset,
				       end_offset - f_end_offset);

				enlarge_entry(cached, end_offset - f_end_offset);
				f->length = f->real_length = end_offset - f->offset;
			}

		} else {
			/* We will just discard this, it's complete subset of
			 * our new fragment. */
			if (memcmp(f->data + f->next->offset - f->offset,
				   f->next->data,
				   f->next->length))
				*trunc = 1;
		}

		/* Remove the fragment, it influences our new one! */
		nf = f->next;
		enlarge_entry(cached, -nf->length);
		del_from_list(nf);
		frag_free(nf);
	}
}

/* Note that this function is maybe overcommented, but I'm certainly not
 * unhappy from that. */
int
add_fragment(struct cache_entry *cached, off_t offset,
	     const unsigned char *data, ssize_t length)
{
	struct fragment *f, *nf;
	int trunc = 0;
	off_t end_offset;

	if (!length) return 0;

	end_offset = offset + length;
	if (cached->length < end_offset)
		cached->length = end_offset;

	/* id marks each entry, and change each time it's modified,
	 * used in HTML renderer. */
	cached->cache_id = id_counter++;

	/* Possibly insert the new data in the middle of existing fragment. */
	foreach (f, cached->frag) {
		int ret = 0;
		off_t f_end_offset = f->offset + f->length;

		/* No intersection? */
		if (f->offset > offset) break;
		if (f_end_offset < offset) continue;

		if (end_offset > f_end_offset) {
			/* Overlap - we end further than original fragment. */

			if (end_offset - f->offset <= f->real_length) {
				/* We fit here, so let's enlarge it by delta of
				 * old and new end.. */
				enlarge_entry(cached, end_offset - f_end_offset);
				/* ..and length is now total length. */
				f->length = end_offset - f->offset;

				ret = 1; /* It was enlarged. */
			} else {
				/* We will reduce fragment length only to the
				 * starting non-interjecting size and add new
				 * fragment directly after this one. */
				f->length = offset - f->offset;
				f = f->next;
				break;
			}

		} /* else We are subset of original fragment. */

		/* Copy the stuff over there. */
		memcpy(f->data + offset - f->offset, data, length);

		remove_overlaps(cached, f, &trunc);

		/* We truncate the entry even if the data contents is the
		 * same as what we have in the fragment, because that does
		 * not mean that what is going to follow won't differ, This
		 * is a serious problem when rendering HTML frame with onload
		 * snippets - we "guess" the rest of the document here,
		 * interpret the snippet, then it turns out in the real
		 * document the snippet is different and we are in trouble.
		 *
		 * Debugging this took me about 1.5 day (really), the diff with
		 * all the debugging print commands amounted about 20kb (gdb
		 * wasn't much useful since it stalled the download, de facto
		 * eliminating the bad behaviour). */
		truncate_entry(cached, end_offset, 0);

		dump_frags(cached, "add_fragment");

		return ret;
	}

	/* Make up new fragment. */
	nf = frag_alloc(CACHE_PAD(length));
	if (!nf) return -1;

	nf->offset = offset;
	nf->length = length;
	nf->real_length = CACHE_PAD(length);
	memcpy(nf->data, data, length);
	add_at_pos(f->prev, nf);

	enlarge_entry(cached, length);

	remove_overlaps(cached, nf, &trunc);
	if (trunc) truncate_entry(cached, end_offset, 0);

	dump_frags(cached, "add_fragment");

	return 1;
}

/* Try to defragment the cache entry. Defragmentation will not be possible
 * if there is a gap in the fragments; if we have bytes 1-100 in one fragment
 * and bytes 201-300 in the second, we must leave those two fragments separate
 * so that the fragment for bytes 101-200 can later be inserted. However,
 * if we have the fragments for bytes 1-100, 101-200, and 201-300, we will
 * catenate them into one new fragment and replace the original fragments
 * with that new fragment.
 *
 * If are no fragments, return NULL. If there is no fragment with byte 1,
 * return NULL. Otherwise, return the first fragment, whether or not it was
 * possible to fully defragment the entry. */
struct fragment *
get_cache_fragment(struct cache_entry *cached)
{
	struct fragment *first_frag, *adj_frag, *frag, *new_frag;
	int new_frag_len;

	if (list_empty(cached->frag))
		return NULL;

	first_frag = cached->frag.next;
	if (first_frag->offset)
		return NULL;

	/* Only one fragment so no defragmentation is needed */
	if (list_is_singleton(cached->frag))
		return first_frag;

	/* Find the first pair of fragments with a gap in between. Only
	 * fragments up to the first gap can be defragmented. */
	for (adj_frag = first_frag->next; adj_frag != (void *) &cached->frag;
	     adj_frag = adj_frag->next) {
		long gap = adj_frag->offset
			    - (adj_frag->prev->offset + adj_frag->prev->length);

		if (gap > 0) break;
		if (gap == 0) continue;

		INTERNAL("fragments overlap");
		return NULL;
	}

	/* There is a gap between the first two fragments, so we can't
	 * defragment anything. */
	if (adj_frag == first_frag->next)
		return first_frag;

	/* Calculate the length of the defragmented fragment. */
	for (new_frag_len = 0, frag = first_frag;
	     frag != adj_frag;
	     frag = frag->next)
		new_frag_len += frag->length;

	/* XXX: If the defragmentation fails because of allocation failure,
	 * fall back to return the first fragment and pretend all is well. */
	/* FIXME: Is this terribly brain-dead? It corresponds to the semantic of
	 * the code this extended version of the old defrag_entry() is supposed
	 * to replace. --jonas */
	new_frag = frag_alloc(new_frag_len);
	if (!new_frag)
		return first_frag->length ? first_frag : NULL;

	new_frag->length = new_frag_len;
	new_frag->real_length = new_frag_len;

	for (new_frag_len = 0, frag = first_frag;
	     frag != adj_frag;
	     frag = frag->next) {
		struct fragment *tmp = frag;

		memcpy(new_frag->data + new_frag_len, frag->data, frag->length);
		new_frag_len += frag->length;

		frag = frag->prev;
		del_from_list(tmp);
		frag_free(tmp);
	}

	add_to_list(cached->frag, new_frag);

	dump_frags(cached, "get_cache_fragment");

	return new_frag;
}

static void
delete_fragment(struct cache_entry *cached, struct fragment *f)
{
	while ((void *) f != &cached->frag) {
		struct fragment *tmp = f->next;

		enlarge_entry(cached, -f->length);
		del_from_list(f);
		frag_free(f);
		f = tmp;
	}
}

static void
truncate_entry(struct cache_entry *cached, off_t offset, int final)
{
	struct fragment *f;

	if (cached->length > offset) {
		cached->length = offset;
		cached->incomplete = 1;
	}

	foreach (f, cached->frag) {
		off_t size = offset - f->offset;

		/* XXX: is zero length fragment really legal here ? --Zas */
		assert(f->length >= 0);

		if (size >= f->length) continue;

		if (size > 0) {
			enlarge_entry(cached, -(f->length - size));
			f->length = size;

			if (final) {
				struct fragment *nf;

				nf = frag_realloc(f, f->length);
				if (nf) {
					nf->next->prev = nf;
					nf->prev->next = nf;
					f = nf;
					f->real_length = f->length;
				}
			}

			f = f->next;
		}

		delete_fragment(cached, f);

		dump_frags(cached, "truncate_entry");
		return;
	}
}

void
free_entry_to(struct cache_entry *cached, off_t offset)
{
	struct fragment *f;

	foreach (f, cached->frag) {
		if (f->offset + f->length <= offset) {
			struct fragment *tmp = f;

			enlarge_entry(cached, -f->length);
			f = f->prev;
			del_from_list(tmp);
			frag_free(tmp);
		} else if (f->offset < offset) {
			off_t size = offset - f->offset;

			enlarge_entry(cached, -size);
			f->length -= size;
			memmove(f->data, f->data + size, f->length);
			f->offset = offset;
		} else break;
	}
}

void
delete_entry_content(struct cache_entry *cached)
{
	enlarge_entry(cached, -cached->data_size);

	while (cached->frag.next != (void *) &cached->frag) {
		struct fragment *f = cached->frag.next;

		del_from_list(f);
		frag_free(f);
	}
	cached->cache_id = id_counter++;
	cached->length = 0;
	cached->incomplete = 1;

	mem_free_set(&cached->last_modified, NULL);
	mem_free_set(&cached->etag, NULL);
}

static void
done_cache_entry(struct cache_entry *cached)
{
	assertm(!is_object_used(cached), "deleting locked cache entry");
	assertm(!is_entry_used(cached), "deleting loading cache entry");

	delete_entry_content(cached);

	if (cached->box_item) done_listbox_item(&cache_browser, cached->box_item);
#ifdef CONFIG_SCRIPTING_SPIDERMONKEY
	if (cached->jsobject) smjs_detach_cache_entry_object(cached);
#endif

	if (cached->uri) done_uri(cached->uri);
	if (cached->proxy_uri) done_uri(cached->proxy_uri);
	if (cached->redirect) done_uri(cached->redirect);

	mem_free_if(cached->head);
	mem_free_if(cached->content_type);
	mem_free_if(cached->last_modified);
	mem_free_if(cached->ssl_info);
	mem_free_if(cached->encoding_info);
	mem_free_if(cached->etag);

	mem_free(cached);
}

void
delete_cache_entry(struct cache_entry *cached)
{
	del_from_list(cached);

	done_cache_entry(cached);
}


void
normalize_cache_entry(struct cache_entry *cached, off_t truncate_length)
{
	if (truncate_length < 0)
		return;

	truncate_entry(cached, truncate_length, 1);
	cached->incomplete = 0;
	cached->preformatted = 0;
	cached->seconds = time(NULL);
}


struct uri *
redirect_cache(struct cache_entry *cached, unsigned char *location,
	       int get, int incomplete)
{
	unsigned char *uristring;

	/* XXX: I am a little puzzled whether we should only use the cache
	 * entry's URI if it is valid. Hopefully always using it won't hurt.
	 * Currently we handle direction redirects where "/" should be appended
	 * special dunno if join_urls() could be made to handle that.
	 * --jonas */
	/* XXX: We are assuming here that incomplete will only be zero when
	 * doing these fake redirects which only purpose is to add an ending
	 * slash *cough* dirseparator to the end of the URI. */
	if (incomplete == 0 && dir_sep(location[0]) && location[1] == 0) {
		/* To be sure use get_uri_string() to get rid of post data */
		uristring = get_uri_string(cached->uri, URI_ORIGINAL);
		if (uristring) add_to_strn(&uristring, location);
	} else {
		uristring = join_urls(cached->uri, location);
	}

	if (!uristring) return NULL;

	/* Only add the post data if the redirect should not use GET method.
	 * This is tied to the HTTP handling of the 303 and (if the
	 * protocol.http.bugs.broken_302_redirect is enabled) the 302 status
	 * code handling. */
	if (cached->uri->post
	    && !cached->redirect_get
	    && !get) {
		/* XXX: Add POST_CHAR and post data assuming URI components
		 * belong to one string. */

		/* To be certain we don't append post data twice in some
		 * conditions... --Zas */
		assert(!strchr(uristring, POST_CHAR));

		add_to_strn(&uristring, cached->uri->post - 1);
	}

	if (cached->redirect) done_uri(cached->redirect);
	cached->redirect = get_uri(uristring, 0);
	cached->redirect_get = get;
	if (incomplete >= 0) cached->incomplete = incomplete;

	mem_free(uristring);

	return cached->redirect;
}


void
garbage_collection(int whole)
{
	struct cache_entry *cached;
	/* We recompute cache_size when scanning cache entries, to ensure
	 * consistency. */
	unsigned longlong old_cache_size = 0;
	/* The maximal cache size tolerated by user. Note that this is only
	 * size of the "just stored" unused cache entries, used cache entries
	 * are not counted to that. */
	unsigned longlong opt_cache_size = get_opt_long("document.cache.memory.size");
	/* The low-treshold cache size. Basically, when the cache size is
	 * higher than opt_cache_size, we free the cache so that there is no
	 * more than this value in the cache anymore. This is to make sure we
	 * aren't cleaning cache too frequently when working with a lot of
	 * small cache entries but rather free more and then let it grow a
	 * little more as well. */
	unsigned longlong gc_cache_size = opt_cache_size * MEMORY_CACHE_GC_PERCENT / 100;
	/* The cache size we aim to reach. */
	unsigned longlong new_cache_size = cache_size;
#ifdef DEBUG_CACHE
	/* Whether we've hit an used (unfreeable) entry when collecting
	 * garbage. */
	int obstacle_entry = 0;
#endif

#ifdef DEBUG_CACHE
	DBG("gc whole=%d opt_cache_size=%ld gc_cache_size=%ld",
	      whole, opt_cache_size,gc_cache_size);
#endif

	if (!whole && cache_size <= opt_cache_size) return;


	/* Scanning cache, pass #1:
	 * Weed out the used cache entries from @new_cache_size, so that we
	 * will work only with the unused entries from then on. Also ensure
	 * that @cache_size is in sync. */

	foreach (cached, cache_entries) {
		old_cache_size += cached->data_size;

		if (!is_object_used(cached) && !is_entry_used(cached))
			continue;

		assertm(new_cache_size >= cached->data_size,
			"cache_size (%ld) underflow: subtracting %ld from %ld",
			cache_size, cached->data_size, new_cache_size);

		new_cache_size -= cached->data_size;

		if_assert_failed { new_cache_size = 0; }
	}

	assertm(old_cache_size == cache_size,
		"cache_size out of sync: %ld != (actual) %ld",
		cache_size, old_cache_size);
	if_assert_failed { cache_size = old_cache_size; }

	if (!whole && new_cache_size <= opt_cache_size) return;


	/* Scanning cache, pass #2:
	 * Mark potential targets for destruction, from the oldest to the
	 * newest. */

	foreachback (cached, cache_entries) {
		/* We would have shrinked enough already? */
		if (!whole && new_cache_size <= gc_cache_size)
			goto shrinked_enough;

		/* Skip used cache entries. */
		if (is_object_used(cached) || is_entry_used(cached)) {
#ifdef DEBUG_CACHE
			obstacle_entry = 1;
#endif
			cached->gc_target = 0;
			continue;
		}

		/* FIXME: Optionally take cached->max_age into consideration,
		 * but that will probably complicate things too much. We'd have
		 * to sort entries so prioritize removing the oldest entries. */

		assertm(new_cache_size >= cached->data_size,
			"cache_size (%ld) underflow: subtracting %ld from %ld",
			cache_size, cached->data_size, new_cache_size);

		/* Mark me for destruction, sir. */
		cached->gc_target = 1;
		new_cache_size -= cached->data_size;

		if_assert_failed { new_cache_size = 0; }
	}

	/* If we'd free the whole cache... */
	assertm(new_cache_size == 0,
		"cache_size (%ld) overflow: %ld",
		cache_size, new_cache_size);
	if_assert_failed { new_cache_size = 0; }

shrinked_enough:


	/* Now turn around and start walking in the opposite direction. */
	cached = cached->next;

	/* Something is strange when we decided all is ok before dropping any
	 * cache entry. */
	if ((void *) cached == &cache_entries) return;


	if (!whole) {
		struct cache_entry *entry;

		/* Scanning cache, pass #3:
		 * Walk back in the cache and unmark the cache entries which
		 * could still fit into the cache. */

		/* This makes sense when the newest entry is HUGE and after it,
		 * there's just plenty of tiny entries. By this point, all the
		 * tiny entries would be marked for deletion even though it'd
		 * be enough to free the huge entry. This actually fixes that
		 * situation. */

		for (entry = cached; (void *) entry != &cache_entries; entry = entry->next) {
			unsigned longlong newer_cache_size = new_cache_size + entry->data_size;

			if (newer_cache_size > gc_cache_size)
				continue;

			new_cache_size = newer_cache_size;
			entry->gc_target = 0;
		}
	}


	/* Scanning cache, pass #4:
	 * Destroy the marked entries. So sad, but that's life, bro'. */

	for (; (void *) cached != &cache_entries; ) {
		cached = cached->next;
		if (cached->prev->gc_target)
			delete_cache_entry(cached->prev);
	}


#ifdef DEBUG_CACHE
	if ((whole || !obstacle_entry) && cache_size > gc_cache_size) {
		DBG("garbage collection doesn't work, cache size %ld > %ld, "
		      "document.cache.memory.size set to: %ld bytes",
		      cache_size, gc_cache_size,
		      get_opt_long("document.cache.memory.size"));
	}
#endif
}
