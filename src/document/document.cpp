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

#ifdef CONFIG_ECMASCRIPT
#include "ecmascript/ecmascript.h"
#endif
#ifdef CONFIG_ECMASCRIPT_SMJS
#include "ecmascript/spidermonkey.h"
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
#ifdef CONFIG_LIBSIXEL
#include "terminal/sixel.h"
#endif
#include "util/color.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"

#include <list>

static std::list<struct document *> format_cache;

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

#if 0
static void
dump_format_cache(int line)
{
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

#ifdef CONFIG_LIBDOM
	init_string(&document->text);
#endif

#ifdef CONFIG_ECMASCRIPT
	init_list(document->onload_snippets);
	init_list(document->timeouts);
#endif

#ifdef CONFIG_LIBSIXEL
	init_list(document->images);
#endif

#ifdef CONFIG_COMBINE
	document->comb_x = -1;
	document->comb_y = -1;
#endif
	object_nolock(document, "document");
	object_lock(document);

	copy_opt(&document->options, options);
	format_cache.push_back(document);

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

static void
free_iframeset_desc(struct iframeset_desc *iframeset_desc)
{
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
reset_document(struct document *document)
{
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
///	kill_timer(&document->timeout);
///	free_document(document->dom);
#endif

	free_list(document->tags);
	free_list(document->nodes);

	mem_free_set(&document->search, NULL);
	mem_free_set(&document->slines1, NULL);
	mem_free_set(&document->slines2, NULL);
	mem_free_set(&document->search_points, NULL);

#ifdef CONFIG_COMBINE
	discard_comb_x_y(document);
#endif
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
	if (document->iframe_desc) free_iframeset_desc(document->iframe_desc);
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
			mem_free_if(document->data[pos].ch.chars);

		mem_free(document->data);
	}

	mem_free_if(document->lines1);
	mem_free_if(document->lines2);
	done_document_options(&document->options);

	while (!list_empty(document->forms)) {
		done_form((struct form *)document->forms.next);
	}

#ifdef CONFIG_LIBSIXEL
	while (!list_empty(document->images)) {
		delete_image((struct image *)document->images.next);
	}
#endif

#ifdef CONFIG_CSS
	free_uri_list(&document->css_imports);
#endif
#if defined(CONFIG_ECMASCRIPT_SMJS) || defined(CONFIG_QUICKJS) || defined(CONFIG_MUJS)
	free_ecmascript_string_list(&document->onload_snippets);
	free_uri_list(&document->ecmascript_imports);

	{
		struct ecmascript_timeout *t;

		foreach(t, document->timeouts) {
			kill_timer(&t->tid);
			done_string(&t->code);
		}
	}
	free_list(document->timeouts);
	mem_free_if(document->body_onkeypress);
#endif

#ifdef CONFIG_LIBDOM
	done_string(&document->text);
	free_document(document->dom);

	if (document->element_map) {
		void *mapa = document->element_map;

		clear_map(mapa);
		delete_map(mapa);
	}

	if (document->element_map_rev) {
		void *mapa = document->element_map_rev;

		clear_map_rev(mapa);
		delete_map_rev(mapa);
	}
#endif
	free_list(document->tags);
	free_list(document->nodes);

	mem_free_if(document->search);
	mem_free_if(document->slines1);
	mem_free_if(document->slines2);
	mem_free_if(document->search_points);

	format_cache.remove(document);
	mem_free(document);
}

void
release_document(struct document *document)
{
	assert(document);
	if_assert_failed return;

	if (document->refresh) kill_document_refresh(document->refresh);
#ifdef CONFIG_ECMASCRIPT
	{
		struct ecmascript_timeout *t;

		foreach(t, document->timeouts) {
			kill_timer(&t->tid);
			done_string(&t->code);
		}
	}
	free_list(document->timeouts);
#endif
	object_unlock(document);

	format_cache.remove(document);
	format_cache.push_front(document);
}

int
find_tag(struct document *document, char *name, int namelen)
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

#ifdef CONFIG_CSS
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
	active_link.insert_mode_color.foreground = get_opt_color("document.browse.links.active_link.insert_mode_colors.text", ses);
	active_link.insert_mode_color.background = get_opt_color("document.browse.links.active_link.insert_mode_colors.background", ses);
	active_link.enable_color = get_opt_bool("document.browse.links.active_link.enable_color", ses);
	active_link.invert = get_opt_bool("document.browse.links.active_link.invert", ses);
	active_link.underline = get_opt_bool("document.browse.links.active_link.underline", ses);
	active_link.bold = get_opt_bool("document.browse.links.active_link.bold", ses);

	for (auto it = format_cache.begin(); it != format_cache.end(); it++) {
		document = *it;
		copy_struct(&document->options.active_link, &active_link);
	}
}

struct document *
get_cached_document(struct cache_entry *cached, struct document_options *options)
{
	struct document *ret = NULL;
	std::list<struct document *> to_remove;

	for (auto it = format_cache.begin(); it != format_cache.end(); it++) {
		struct document *document = *it;

		if (!compare_uri(document->uri, cached->uri, 0)
		    || compare_opt(&document->options, options))
			continue;

		if (options->no_cache
		    || cached->cache_id != document->cache_id
		    || !check_document_css_magic(document)) {
			if (!is_object_used(document)) {
				to_remove.push_back(document);
			}
			continue;
		}
		ret = document;
		break;
	}
	for (auto it = to_remove.begin(); it != to_remove.end(); it++) {
		done_document(*it);
	}
	to_remove.clear();

	if (ret) {
		/* Reactivate */
		format_cache.remove(ret);
		format_cache.push_front(ret);
		object_lock(ret);

		return ret;
	}

	return NULL;
}

void
shrink_format_cache(int whole)
{
	struct document *document;
	int format_cache_size = get_opt_int("document.cache.format.size", NULL);
	int format_cache_entries = 0;
	std::list<struct document *> to_remove;

	for (auto it = format_cache.begin(); it != format_cache.end(); it++) {
		document = *it;

		if (is_object_used(document)) continue;

		format_cache_entries++;

		/* Destroy obsolete renderer documents which are already
		 * out-of-sync. */
		if (document->cached->cache_id == document->cache_id)
			continue;

		to_remove.push_back(document);
		format_cache_entries--;
	}

	assertm(format_cache_entries >= 0, "format_cache_entries underflow on entry");
	if_assert_failed format_cache_entries = 0;

	for (auto it = to_remove.begin(); it != to_remove.end(); it++) {
		done_document(*it);
	}
	to_remove.clear();

	for (auto it = format_cache.rbegin(); it != format_cache.rend(); it++) {
		document = *it;

		if (is_object_used(document)) continue;

		/* If we are not purging the whole format cache, stop
		 * once we are below the maximum number of entries. */
		if (!whole && format_cache_entries <= format_cache_size)
			break;

		to_remove.push_back(document);
		format_cache_entries--;
	}
	for (auto it = to_remove.begin(); it != to_remove.end(); it++) {
		done_document(*it);
	}
	to_remove.clear();

	assertm(format_cache_entries >= 0, "format_cache_entries underflow");
	if_assert_failed format_cache_entries = 0;
}

int
get_format_cache_size(void)
{
	return format_cache.size();
}

int
get_format_cache_used_count(void)
{
	struct document *document;
	int i = 0;

	for (auto it = format_cache.begin(); it != format_cache.end(); it++) {
		document = *it;
		i += is_object_used(document);
	}
	return i;
}

int
get_format_cache_refresh_count(void)
{
	struct document *document;
	int i = 0;

	for (auto it = format_cache.begin(); it != format_cache.end(); it++) {
		document = *it;

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
	init_tags_lookup();
}

static void
done_documents(struct module *module)
{
	free_tags_lookup();
	free_table_cache();
}

#ifdef CONFIG_ECMASCRIPT
int
get_link_number_by_offset(struct document *document, int offset)
{
	int link;

	if (!document->links_sorted) sort_links(document);

	for (link = 0; link < document->nlinks; link++) {
		if (document->links[link].element_offset == offset) {
			return link;
		}
	}

	return -1;
}
#endif

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
