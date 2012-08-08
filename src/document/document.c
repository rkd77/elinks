/** The document base functionality
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include "document/document.h"
#include "document/forms.h"
#include "document/html/frames.h"
#include "document/html/parser.h"
#include "document/html/parser/parse.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "document/refresh.h"
#include "main/module.h"
#include "main/object.h"
#include "network/dns.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"

static INIT_LIST_OF(struct document, format_cache);

#ifdef HAVE_INET_NTOP
/* DNS callback. */
static void
found_dns(void *data, struct sockaddr_storage *addr, int addrlen)
{
	unsigned char buf[64];
	const unsigned char *res;
	struct sockaddr *s;
	unsigned char **ip = (unsigned char **)data;
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
#ifdef HAVE_INET_NTOP
	struct uri *uri = document->uri;
	char tmp;

	if (!uri || !uri->host || !uri->hostlen) return;
	tmp = uri->host[uri->hostlen];
	uri->host[uri->hostlen] = 0;
	find_host(uri->host, &document->querydns, found_dns, &document->ip, 0);
	uri->host[uri->hostlen] = tmp;
#endif
}

struct document *
init_document(struct cache_entry *cached, struct document_options *options)
{
	struct document *document = mem_calloc(1, sizeof(*document));

	if (!document) return NULL;

	document->uri = get_uri_reference(cached->uri);

	get_ip(document);

	object_lock(cached);
	document->cache_id = cached->cache_id;
	document->cached = cached;

	init_list(document->forms);
	init_list(document->tags);
	init_list(document->nodes);

#ifdef CONFIG_ECMASCRIPT
	init_list(document->onload_snippets);
#endif

#ifdef CONFIG_COMBINE
	document->comb_x = -1;
	document->comb_y = -1;
#endif
	object_nolock(document, "document");
	object_lock(document);

	copy_opt(&document->options, options);

	add_to_list(format_cache, document);

	return document;
}

static void
free_frameset_desc(struct frameset_desc *frameset_desc)
{
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

void
done_link_members(struct link *link)
{
	if (link->event_hooks) {
		struct script_event_hook *evhook, *safety;

		foreachsafe (evhook, safety, *link->event_hooks) {
			mem_free_if(evhook->src);
			mem_free(evhook);
		}
		mem_free(link->event_hooks);
	}
	mem_free_if(get_link_name(link));
	mem_free_if(link->where);
	mem_free_if(link->target);
	mem_free_if(link->title);
	mem_free_if(link->where_img);
	mem_free_if(link->points);
}

void
done_document(struct document *document)
{
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
	if (document->refresh) done_document_refresh(document->refresh);

	if (document->links) {
		int pos;

		for (pos = 0; pos < document->nlinks; pos++)
			done_link_members(&document->links[pos]);

		mem_free(document->links);
	}

	if (document->data) {
		int pos;

		for (pos = 0; pos < document->height; pos++)
			mem_free_if(document->data[pos].chars);

		mem_free(document->data);
	}

	mem_free_if(document->lines1);
	mem_free_if(document->lines2);
	done_document_options(&document->options);

	while (!list_empty(document->forms)) {
		done_form(document->forms.next);
	}

#ifdef CONFIG_CSS
	free_uri_list(&document->css_imports);
#endif
#ifdef CONFIG_ECMASCRIPT
	free_string_list(&document->onload_snippets);
	free_uri_list(&document->ecmascript_imports);
	kill_timer(&document->timeout);
#endif

	free_list(document->tags);
	free_list(document->nodes);

	mem_free_if(document->search);
	mem_free_if(document->slines1);
	mem_free_if(document->slines2);

	del_from_list(document);
	mem_free(document);
}

void
release_document(struct document *document)
{
	assert(document);
	if_assert_failed return;

	if (document->refresh) kill_document_refresh(document->refresh);
#ifdef CONFIG_ECMASCRIPT
	kill_timer(&document->timeout);
#endif
	object_unlock(document);
	move_to_top_of_list(format_cache, document);
}

int
find_tag(struct document *document, unsigned char *name, int namelen)
{
	struct tag *tag;

	foreach (tag, document->tags)
		if (!c_strlcasecmp(tag->name, -1, name, namelen))
			return tag->y;

	return -1;
}

/* Formatted document cache management */

/* ECMAScript doesn't like anything like CSS since it doesn't modify the
 * formatted document (yet). */

#if CONFIG_CSS
unsigned long
get_document_css_magic(struct document *document)
{
	unsigned long css_magic = 0;
	struct uri *uri;
	int index;

	foreach_uri (uri, index, &document->css_imports) {
		struct cache_entry *cached = find_in_cache(uri);

		if (cached) css_magic += cached->cache_id + cached->data_size;
	}

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
	struct document *document;
	struct active_link_options active_link;

	memset(&active_link, 0, sizeof(active_link));	/* Safer. */
	active_link.color.foreground = get_opt_color("document.browse.links.active_link.colors.text", ses);
	active_link.color.background = get_opt_color("document.browse.links.active_link.colors.background", ses);
	active_link.enable_color = get_opt_bool("document.browse.links.active_link.enable_color", ses);
	active_link.invert = get_opt_bool("document.browse.links.active_link.invert", ses);
	active_link.underline = get_opt_bool("document.browse.links.active_link.underline", ses);
	active_link.bold = get_opt_bool("document.browse.links.active_link.bold", ses);

	foreach (document, format_cache) {
		copy_struct(&document->options.active_link, &active_link);
	}
}

struct document *
get_cached_document(struct cache_entry *cached, struct document_options *options)
{
	struct document *document, *next;

	foreachsafe (document, next, format_cache) {
		if (!compare_uri(document->uri, cached->uri, 0)
		    || compare_opt(&document->options, options))
			continue;

		if (options->no_cache
		    || cached->cache_id != document->cache_id
		    || !check_document_css_magic(document)) {
			if (!is_object_used(document)) {
				done_document(document);
			}
			continue;
		}

		/* Reactivate */
		move_to_top_of_list(format_cache, document);

		object_lock(document);

		return document;
	}

	return NULL;
}

void
shrink_format_cache(int whole)
{
	struct document *document, *next;
	int format_cache_size = get_opt_int("document.cache.format.size", NULL);
	int format_cache_entries = 0;

	foreachsafe (document, next, format_cache) {
		if (is_object_used(document)) continue;

		format_cache_entries++;

		/* Destroy obsolete renderer documents which are already
		 * out-of-sync. */
		if (document->cached->cache_id == document->cache_id)
			continue;

		done_document(document);
		format_cache_entries--;
	}

	assertm(format_cache_entries >= 0, "format_cache_entries underflow on entry");
	if_assert_failed format_cache_entries = 0;

	foreachbacksafe (document, next, format_cache) {
		if (is_object_used(document)) continue;

		/* If we are not purging the whole format cache, stop
		 * once we are below the maximum number of entries. */
		if (!whole && format_cache_entries <= format_cache_size)
			break;

		done_document(document);
		format_cache_entries--;
	}

	assertm(format_cache_entries >= 0, "format_cache_entries underflow");
	if_assert_failed format_cache_entries = 0;
}

int
get_format_cache_size(void)
{
	return list_size(&format_cache);
}

int
get_format_cache_used_count(void)
{
	struct document *document;
	int i = 0;

	foreach (document, format_cache)
		i += is_object_used(document);
	return i;
}

int
get_format_cache_refresh_count(void)
{
	struct document *document;
	int i = 0;

	foreach (document, format_cache)
		if (document->refresh
		    && document->refresh->timer != TIMER_ID_UNDEF)
			i++;
	return i;
}

static void
init_documents(struct module *module)
{
	init_tags_lookup();
}

static void
done_documents(struct module *module)
{
	free_tags_lookup();
	free_table_cache();
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
	/* done: */		done_documents
);
