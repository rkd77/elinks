/** The document base functionality
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_IDNA_H
#include <idna.h>
#endif

#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h> /* socklen_t for MinGW */
#endif

#ifdef HAVE_GETIFADDRS
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>		/* getifaddrs() */
#endif
#endif				/* HAVE_GETIFADDRS */

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/html/frames.h"
#include "document/html/iframes.h"
#include "document/html/parser.h"
#include "document/html/parser/parse.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "document/refresh.h"
#include "document/renderer.h"

#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
#include "js/ecmascript-c.h"
#endif

#ifdef CONFIG_LIBDOM
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#endif

#include "main/module.h"
#include "main/object.h"
#include "network/dns.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#ifdef CONFIG_KITTY
#include "terminal/kitty.h"
#endif
#ifdef CONFIG_LIBSIXEL
#include "terminal/sixel.h"
#endif
#include "util/color.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"

struct document_list {
	LIST_HEAD_EL(struct document_list);
	struct document *document;
};

static INIT_LIST_OF(struct document_list, format_cache);

const char *script_event_hook_name[] = {
	"click",
	"dblclick",
	"mouseover",
	"hover",
	"focus",
	"mouseout",
	"blur",
	"keydown",
	"keyup",
	"keypress",
	"keypress",
	NULL
};

static void add_to_document_list(LIST_OF(struct document_list) *list, struct document *document);

static void
remove_document_from_format_cache(struct document *document)
{
	ELOG
	struct document_list *item;

	foreach (item, format_cache) {
		if (item->document == document) {
			del_from_list(item);
			mem_free(item);
			return;
		}
	}
}

static void
move_document_to_top_of_format_cache(struct document *document)
{
	ELOG
	struct document_list *item;

	foreach (item, format_cache) {
		if (item->document == document) {
			move_to_top_of_list(format_cache, item);
			return;
		}
	}
}

#if 0
static void
dump_format_cache(int line)
{
	ELOG
	fprintf(stderr, "line=%d size=%ld", line, format_cache.size());
	for (auto it = format_cache.begin(); it != format_cache.end(); it++) {
		fprintf(stderr, " %p", *it);
	}
	fprintf(stderr, "\n");
}
#endif

#ifdef HAVE_INET_NTOP
/* DNS callback. */
static void
found_dns(void *data, struct sockaddr_storage *addr, int addrlen)
{
	ELOG
	char buf[64];
	const char *res;
	struct sockaddr *s;
	char **ip = (char **)data;
	void *src;

	if (!ip || !addr) return;
	s = (struct sockaddr *)addr;
	if (s->sa_family == AF_INET6) {
		src = &(((struct sockaddr_in6 *)s)->sin6_addr.s6_addr);
	} else {
		src = &(((struct sockaddr_in *)s)->sin_addr.s_addr);
	}
	res = inet_ntop(s->sa_family, src, buf, 64);
	if (res) {
		*ip = stracpy(res);
	}
}
#endif

static void
get_ip(struct document *document)
{
	ELOG
#ifdef HAVE_INET_NTOP
	struct uri *uri = document->uri;
	char *host = get_uri_string(uri, URI_DNS_HOST);

	if (host) {
		find_host(host, &document->querydns, found_dns, &document->ip, 0);
		mem_free(host);
	}
#endif
}

struct document *
init_document(struct cache_entry *cached, struct document_options *options)
{
	ELOG
	struct document *document = (struct document *)mem_calloc(1, sizeof(*document));

	if (!document) return NULL;

	document->uri = get_uri_reference(cached->uri);

	get_ip(document);

	object_lock(cached);
	document->cache_id = cached->cache_id;
	document->cached = cached;

	init_list(document->forms);
	init_list(document->tags);
	init_list(document->nodes);
	init_list(document->iframes);

#ifdef CONFIG_LIBDOM
	init_string(&document->text);
#endif

#ifdef CONFIG_ECMASCRIPT
	init_list(document->onload_snippets);
#endif

#ifdef CONFIG_KITTY
	init_list(document->k_images);
#endif

#ifdef CONFIG_LIBSIXEL
	init_list(document->images);
#endif

	object_nolock(document, "document");
	object_lock(document);

	copy_opt(&document->options, options);
	add_to_document_list(&format_cache, document);

	return document;
}

static void
free_frameset_desc(struct frameset_desc *frameset_desc)
{
	ELOG
	int i;

	for (i = 0; i < frameset_desc->n; i++) {
		struct frame_desc *frame_desc = &frameset_desc->frame_desc[i];

		if (frame_desc->subframe)
			free_frameset_desc(frame_desc->subframe);
		mem_free_if(frame_desc->name);
		if (frame_desc->uri)
			done_uri(frame_desc->uri);
	}

	mem_free(frameset_desc);
}

static void
free_iframeset_desc(struct iframeset_desc *iframeset_desc)
{
	ELOG
	int i;

	for (i = 0; i < iframeset_desc->n; i++) {
		struct iframe_desc *iframe_desc = &iframeset_desc->iframe_desc[i];

//		if (iframe_desc->subframe)
//			free_iframeset_desc(frame_desc->subframe);
		mem_free_if(iframe_desc->name);
		if (iframe_desc->uri)
			done_uri(iframe_desc->uri);
	}

	mem_free(iframeset_desc);
}


void
done_link_members(struct link *link)
{
	ELOG
	if (link->event_hooks) {
		struct script_event_hook *evhook, *safety;

		foreachsafe (evhook, safety, *link->event_hooks) {
			mem_free_if(evhook->src);
			mem_free(evhook);
		}
		mem_free(link->event_hooks);
	}
	if (!link_is_form(link)) {
		mem_free_if(link->data.name);
	}
	mem_free_if(link->where);
	mem_free_if(link->target);
	mem_free_if(link->title);
	mem_free_if(link->where_img);
	mem_free_if(link->points);
}

static void
copy_link(struct link *dest, struct link *src)
{
	ELOG
	dest->accesskey = src->accesskey;
	dest->type = src->type;

#ifdef CONFIG_ECMASCRIPT
	dest->element_offset = src->element_offset; // TODO
#endif
	dest->where = null_or_stracpy(src->where);
	dest->target = null_or_stracpy(src->target);
	dest->where_img = null_or_stracpy(src->where_img);

	dest->title = null_or_stracpy(src->title);
	dest->npoints = src->npoints;

	dest->points = mem_calloc(dest->npoints, sizeof(struct point));

	if (dest->points) {
		memcpy(dest->points, src->points, dest->npoints * (sizeof(struct point)));
	}
	dest->number = src->number;
	dest->tabindex = src->tabindex;
	dest->color = src->color;
	dest->event_hooks = NULL;

	if (src->event_hooks) {
		dest->event_hooks = (LIST_OF(struct script_event_hook) *)mem_calloc(1, sizeof(*dest->event_hooks));

		if (dest->event_hooks) {
			init_list(*dest->event_hooks);
			struct script_event_hook *evhook;

			foreach (evhook, *src->event_hooks) {
				if (!evhook->src) {
					continue;
				}
				struct script_event_hook *copy_evhook = (struct script_event_hook *)mem_calloc(1, sizeof(*copy_evhook));
				if (!copy_evhook) {
					continue;
				}
				copy_evhook->type = evhook->type;
				copy_evhook->src = stracpy(evhook->src);
				add_to_list(*(dest->event_hooks), copy_evhook);
			}
		}
	}

	if (!link_is_form(src)) {
		dest->data.name = null_or_stracpy(src->data.name);
	} else {
		dest->data = src->data;
	}
}

static void
copy_line(struct line *dest, struct line *src)
{
	ELOG
	dest->length = src->length;
	dest->ch.chars = mem_alloc(dest->length * sizeof(struct screen_char));

	if (dest->ch.chars) {
		memcpy(dest->ch.chars, src->ch.chars, dest->length * sizeof(struct screen_char));
	}
}

void
reset_document(struct document *document)
{
	ELOG
	assert(document);
	if_assert_failed return;

///	assertm(!is_object_used(document), "Attempt to free locked formatted data.");
///	if_assert_failed return;

	assert(document->cached);
	object_unlock(document->cached);

///	if (document->uri) {
///		done_uri(document->uri);
///		document->uri = NULL;
///	}
///	if (document->querydns) {
///		kill_dns_request(&document->querydns);
///		document->querydns = NULL;
///	}
///	mem_free_set(&document->ip, NULL);
///	mem_free_set(&document->title, NULL);
///	if (document->frame_desc) {
///		free_frameset_desc(document->frame_desc);
///		document->frame_desc = NULL;
///	}
///	if (document->refresh) {
///		done_document_refresh(document->refresh);
///		document->refresh = NULL;
///	}

	if (document->links) {
		int pos;

		for (pos = 0; pos < document->nlinks; pos++)
			done_link_members(&document->links[pos]);

		mem_free_set(&document->links, NULL);
		document->nlinks = 0;
	}
	mem_free_set(&document->reverse_link_lookup, NULL);

	if (document->data) {
		int pos;

		for (pos = 0; pos < document->height; pos++)
			mem_free_if(document->data[pos].ch.chars);

		mem_free_set(&document->data, NULL);
		document->height = 0;
	}

	mem_free_set(&document->lines1, NULL);
	mem_free_set(&document->lines2, NULL);
	document->options.was_xml_parsed = 1;
///	done_document_options(&document->options);

	while (!list_empty(document->forms)) {
		done_form((struct form *)document->forms.next);
	}

#ifdef CONFIG_CSS
	free_uri_list(&document->css_imports);
#endif
#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
	free_ecmascript_string_list(&document->onload_snippets);
	free_uri_list(&document->ecmascript_imports);
	mem_free_set(&document->body_onkeypress, NULL);
	mem_free_set(&document->offset_linknum, NULL);
#endif

	free_list(document->tags);
	free_list(document->nodes);
	free_list(document->iframes);

	mem_free_set(&document->search, NULL);
	mem_free_set(&document->slines1, NULL);
	mem_free_set(&document->slines2, NULL);
	mem_free_set(&document->search_points, NULL);
}

void
done_document(struct document *document)
{
	ELOG
	assert(document);
	if_assert_failed return;

	assertm(!is_object_used(document), "Attempt to free locked formatted data.");
	if_assert_failed return;

	assert(document->cached);
	object_unlock(document->cached);

	if (document->uri) done_uri(document->uri);
	if (document->querydns) kill_dns_request(&document->querydns);
	mem_free_if(document->ip);
	mem_free_if(document->title);
	if (document->frame_desc) free_frameset_desc(document->frame_desc);
	if (document->iframeset_desc) free_iframeset_desc(document->iframeset_desc);
	if (document->refresh) done_document_refresh(document->refresh);

	if (document->links) {
		int pos;

		for (pos = 0; pos < document->nlinks; pos++)
			done_link_members(&document->links[pos]);

		mem_free(document->links);
	}
	mem_free_set(&document->reverse_link_lookup, NULL);

	if (document->data) {
		int pos;

		for (pos = 0; pos < document->height; pos++)
			mem_free_if(document->data[pos].ch.chars);

		mem_free(document->data);
	}

	mem_free_if(document->lines1);
	mem_free_if(document->lines2);
	done_document_options(&document->options);

	while (!list_empty(document->forms)) {
		done_form((struct form *)document->forms.next);
	}

#ifdef CONFIG_KITTY
	while (!list_empty(document->k_images)) {
		delete_k_image((struct k_image *)document->k_images.next);
	}
#endif

#ifdef CONFIG_LIBSIXEL
	while (!list_empty(document->images)) {
		delete_image((struct image *)document->images.next);
	}
#endif

#if defined(CONFIG_KITTY) || defined(CONFIG_LIBSIXEL)
	free_uri_list(&document->image_uris);
#endif

#ifdef CONFIG_CSS
	free_uri_list(&document->css_imports);
#endif
#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
	free_ecmascript_string_list(&document->onload_snippets);
	free_uri_list(&document->ecmascript_imports);
	mem_free_if(document->body_onkeypress);
	mem_free_if(document->offset_linknum);
#endif

#ifdef CONFIG_LIBDOM
	done_string(&document->text);
	free_document(document->dom);

	if (document->element_map) {
		delete_map(document->element_map);
	}

	if (document->element_map_rev) {
		delete_map(document->element_map_rev);
	}
#endif
	free_list(document->tags);
	free_list(document->nodes);
	free_list(document->iframes);

	mem_free_if(document->search);
	mem_free_if(document->slines1);
	mem_free_if(document->slines2);
	mem_free_if(document->search_points);

	remove_document_from_format_cache(document);
	mem_free(document);
}

void
release_document(struct document *document)
{
	ELOG
	assert(document);
	if_assert_failed return;

	if (document->refresh) {
		kill_document_refresh(document->refresh);
	}
	object_unlock(document);
	move_document_to_top_of_format_cache(document);
}

int
find_tag(struct document *document, char *name, int namelen)
{
	ELOG
	struct tag *tag;

	foreach (tag, document->tags)
		if (!c_strlcasecmp(tag->name, -1, name, namelen))
			return tag->y;

	return -1;
}

/* Formatted document cache management */

/* ECMAScript doesn't like anything like CSS since it doesn't modify the
 * formatted document (yet). */

#ifdef CONFIG_CSS
unsigned long
get_document_css_magic(struct document *document)
{
	ELOG
	unsigned long css_magic = 0;
	struct uri *uri;
	int index;

	foreach_uri (uri, index, &document->css_imports) {
		struct cache_entry *cached = find_in_cache(uri);

		if (cached) css_magic += cached->cache_id + cached->data_size;
	}

#if defined(CONFIG_KITTY) || defined(CONFIG_LIBSIXEL)
	foreach_uri (uri, index, &document->image_uris) {
		struct cache_entry *cached = find_in_cache(uri);

		if (cached) css_magic += cached->cache_id + cached->data_size;
	}
#endif

	return css_magic;
}

#define check_document_css_magic(document) \
	((document)->css_magic == get_document_css_magic(document))
#else
#define check_document_css_magic(document) 1
#endif

void
update_cached_document_options(struct session *ses)
{
	ELOG
	struct document *document;
	struct active_link_options active_link;
	struct document_list *item;

	memset(&active_link, 0, sizeof(active_link));	/* Safer. */
	active_link.color.foreground = get_opt_color("document.browse.links.active_link.colors.text", ses);
	active_link.color.background = get_opt_color("document.browse.links.active_link.colors.background", ses);
	active_link.insert_mode_color.foreground = get_opt_color("document.browse.links.active_link.insert_mode_colors.text", ses);
	active_link.insert_mode_color.background = get_opt_color("document.browse.links.active_link.insert_mode_colors.background", ses);
	active_link.enable_color = get_opt_bool("document.browse.links.active_link.enable_color", ses);
	active_link.invert = get_opt_bool("document.browse.links.active_link.invert", ses);
	active_link.underline = get_opt_bool("document.browse.links.active_link.underline", ses);
	active_link.bold = get_opt_bool("document.browse.links.active_link.bold", ses);

	foreach (item, format_cache) {
		document = item->document;
		copy_struct(&document->options.active_link, &active_link);
	}
}

static void
add_to_document_list(LIST_OF(struct document_list) *list, struct document *document)
{
	ELOG
	struct document_list *item = (struct document_list *)mem_alloc(sizeof(*item));

	if (!item) {
		return;
	}
	item->document = document;
	add_to_list_end(*list, item);
}

struct document *
get_cached_document(struct cache_entry *cached, struct document_options *options)
{
	ELOG
	struct document *ret = NULL;
	struct document_list *item, *it;
	INIT_LIST_OF(struct document_list, to_remove);

	foreach (it, format_cache) {
		struct document *document = it->document;

		if (!compare_uri(document->uri, cached->uri, 0)
		    || compare_opt(&document->options, options))
			continue;

		if (
		    cached->cache_id != document->cache_id
		    || !check_document_css_magic(document)) {
			if (!is_object_used(document)) {
				add_to_document_list(&to_remove, document);
			}
			continue;
		}
		ret = document;
		break;
	}
	foreach (item, to_remove) {
		done_document(item->document);
	}
	free_list(to_remove);

	if (ret) {
		/* Reactivate */
		move_document_to_top_of_format_cache(ret);
		object_lock(ret);

		return ret;
	}

	return NULL;
}

void
shrink_format_cache(int whole)
{
	ELOG
	struct document *document;
	struct document_list *item, *it;
	int format_cache_size = get_opt_int("document.cache.format.size", NULL);
	int format_cache_entries = 0;
	INIT_LIST_OF(struct document_list, to_remove);

	foreach (it, format_cache) {
		document = it->document;

		if (is_object_used(document)) continue;

		format_cache_entries++;

		/* Destroy obsolete renderer documents which are already
		 * out-of-sync. */
		if (document->cached->cache_id == document->cache_id)
			continue;

		add_to_document_list(&to_remove, document);
		format_cache_entries--;
	}

	assertm(format_cache_entries >= 0, "format_cache_entries underflow on entry");
	if_assert_failed format_cache_entries = 0;

	foreach (item, to_remove) {
		done_document(item->document);
	}
	free_list(to_remove);

	foreachback (it, format_cache) {
		document = it->document;

		if (is_object_used(document)) continue;

		/* If we are not purging the whole format cache, stop
		 * once we are below the maximum number of entries. */
		if (!whole && format_cache_entries <= format_cache_size)
			break;

		add_to_document_list(&to_remove, document);
		format_cache_entries--;
	}
	foreach (item, to_remove) {
		done_document(item->document);
	}
	free_list(to_remove);

	assertm(format_cache_entries >= 0, "format_cache_entries underflow");
	if_assert_failed format_cache_entries = 0;
}

int
get_format_cache_size(void)
{
	ELOG
	return list_size(&format_cache);
}

int
get_format_cache_used_count(void)
{
	ELOG
	struct document_list *it;
	struct document *document;
	int i = 0;

	foreach (it, format_cache) {
		document = it->document;
		i += is_object_used(document);
	}
	return i;
}

int
get_format_cache_refresh_count(void)
{
	ELOG
	struct document_list *it;
	struct document *document;
	int i = 0;

	foreach (it, format_cache) {
		document = it->document;

		if (document->refresh
		    && document->refresh->timer != TIMER_ID_UNDEF) {
			i++;
		}
	}
	return i;
}

static void
init_documents(struct module *module)
{
	ELOG
	init_tags_lookup();
}

static void
done_documents(struct module *module)
{
	ELOG
	free_tags_lookup();
	free_table_cache();
}

#ifdef CONFIG_ECMASCRIPT

/* comparison function for qsort() */
static int
comp_offset(const void *v1, const void *v2)
{
	ELOG
	const struct offset_linknum *l1 = (const struct offset_linknum *)v1, *l2 = (const struct offset_linknum *)v2;

	assert(l1 && l2);
	if_assert_failed return 0;

	if (l1->offset < l2->offset) {
		return -1;
	}

	if (l1->offset > l2->offset) {
		return 1;
	}

	return 0;
}

static void
sort_offset(struct document *document)
{
	ELOG
	if (!document->nlinks) {
		return;
	}
	mem_free_set(&document->offset_linknum, malloc(document->nlinks * sizeof(struct offset_linknum)));

	if (!document->offset_linknum) {
		return;
	}
	int i;

	for (i = 0; i < document->nlinks; i++) {
		document->offset_linknum[i].offset = document->links[i].element_offset;
		document->offset_linknum[i].linknum = i;
	}
	qsort(document->offset_linknum, document->nlinks, sizeof(struct offset_linknum), comp_offset);
	document->offset_sorted = 1;
}

int
get_link_number_by_offset(struct document *document, int offset)
{
	ELOG
	if (!document->links_sorted) {
		sort_links(document);
	}

	if (!document->offset_sorted) {
		sort_offset(document);

		if (!document->offset_sorted) {
			return -1;
		}
	}
	struct offset_linknum key = { .offset = offset, .linknum = -1 };
	struct offset_linknum *item = (struct offset_linknum *)bsearch(&key, document->offset_linknum, document->nlinks, sizeof(*item), comp_offset);

	return item ? item->linknum : -1;
}
#endif

void
insert_document_into_document(struct document *dest, struct document *src, int y)
{
	ELOG
	if (!dest || !src || !src->height) {
		return;
	}
	if (y > dest->height) {
		y = dest->height;
	}
	if (!ALIGN_LINES(&dest->data, dest->height, dest->height + src->height)) {
		return;
	}
	memmove(&dest->data[y + src->height], &dest->data[y], (dest->height - y) * sizeof(struct line));

	int i;
	for (i = 0; i < src->height; i++) {
		copy_line(&dest->data[y + i], &src->data[i]);
	}

	if (!ALIGN_LINK(&dest->links, dest->nlinks, dest->nlinks + src->nlinks)) {
		return;
	}

	/* old links */
	int tomove = dest->nlinks;
	for (i = 0; i < dest->nlinks; i++) {
		struct link *link = &dest->links[i];
		int c;
		int found = 0;

		for (c = 0; c < link->npoints; c++) {
			if (link->points[c].y < y) {
				continue;
			};
			link->points[c].y += src->height;
			found = 1;
			if (i < tomove) {
				tomove = i;
			}
		}
		if (found) {
			link->number += src->nlinks;
		}
	}
	/* new links */
	memmove(&dest->links[tomove + src->nlinks], &dest->links[tomove], (dest->nlinks - tomove) * sizeof(struct link));

	for (i = 0; i < src->nlinks; i++) {
		copy_link(&dest->links[tomove + i], &src->links[i]);
	}
	//memcpy(&dest->links[tomove], &src->links[0], src->nlinks * sizeof(struct link));

	for (i = 0; i < src->nlinks; i++) {
		struct link *link = &dest->links[i + tomove];
		int c;

		for (c = 0; c < link->npoints; c++) {
			link->points[c].y += y;
		}
		link->number += tomove;
	}

	/* old images */
#ifdef CONFIG_KITTY
	struct k_image *k_im;
	foreach (k_im, dest->k_images) {
		if (k_im->cy < y) {
			continue;
		};
		k_im->cy += src->height;
	}
	/* new images */
	foreach (k_im, src->k_images) {
		struct k_image *imcopy = mem_calloc(1, sizeof(*imcopy));

		if (!imcopy) {
			continue;
		}
		copy_struct(imcopy, k_im);
		imcopy->cy += y;
		imcopy->pixels->refcnt++;
		add_to_list(dest->k_images, imcopy);
	}
#endif

	/* old images */
#ifdef CONFIG_LIBSIXEL
	struct image *im;
	foreach (im, dest->images) {
		if (im->cy < y) {
			continue;
		};
		im->cy += src->height;
	}
	/* new images */
	foreach (im, src->images) {
		struct image *imcopy = mem_calloc(1, sizeof(*imcopy));

		if (!imcopy) {
			continue;
		}
		copy_struct(imcopy, im);
		imcopy->cy += y;
		add_to_list(dest->images, imcopy);
	}
#endif

	dest->height += src->height;
	dest->nlinks += src->nlinks;
	dest->links_sorted = 0;
	sort_links(dest);
}

void
remove_document_from_document(struct document *dest, struct document *src, int y)
{
	ELOG
	if (!dest || !src || !src->height) {
		return;
	}
	if (y > dest->height) {
		y = dest->height;
	}
	int i;
	for (i = 0; i < src->height; i++) {
		mem_free_if(dest->data[y + i].ch.chars);
	}
	memmove(&dest->data[y], &dest->data[y + src->height], (dest->height - src->height - y) * sizeof(struct line));

	if (!ALIGN_LINES(&dest->data, dest->height, dest->height - src->height)) {
		return;
	}
	/* old links */

	for (i = 0; i < dest->nlinks; i++) {
		struct link *link = &dest->links[i];
		int c;

		for (c = 0; c < link->npoints; c++) {
			if (link->points[c].y >= y && (link->points[c].y < (y + src->height))) {
				link->npoints = 0;
				break;
			};
			if (link->points[c].y >= (y + src->height)) {
				link->points[c].y -= src->height;
			}
		}
	}

	int before = dest->nlinks;
	for (i = 0; i < dest->nlinks; i++) {
		struct link *link = &dest->links[i];

		if (!link->npoints) {
			done_link_members(link);
			memmove(link, link + 1,
				(dest->nlinks - i - 1) * sizeof(*link));
			dest->nlinks--;
			i--;
		}
	}
	//memmove(&dest->links[tomove], &dest->links[tomove + src->nlinks], (src->nlinks) * sizeof(struct link));
	if (!ALIGN_LINK(&dest->links, before, dest->nlinks)) {
		return;
	}

	/* old images */
#ifdef CONFIG_KITTY
	struct k_image *k_im, *k_next;
	foreachsafe (k_im, k_next, dest->k_images) {
		if (k_im->cy >= y && k_im->cy < (y + src->height)) {
			delete_k_image(k_im);
			continue;
		}
		k_im->cy -= src->height;
	}
#endif

	/* old images */
#ifdef CONFIG_LIBSIXEL
	struct image *im, *next;
	foreachsafe (im, next, dest->images) {
		if (im->cy >= y && im->cy < (y + src->height)) {
			del_from_list(im);
			mem_free(im);
			continue;
		}
		im->cy -= src->height;
	}
#endif
	dest->height -= src->height;
	//dest->links_sorted = 0;
	//sort_links(dest);
}

struct module document_module = struct_module(
	/* Because this module is listed in main_modules rather than
	 * in builtin_modules, its name does not appear in the user
	 * interface and so need not be translatable.  */
	/* name: */		"Document",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_documents,
	/* done: */		done_documents,
	/* getname: */	NULL
);
