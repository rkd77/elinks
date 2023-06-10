/* HTML frames parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/html/iframes.h"
#include "document/options.h"
#include "document/renderer.h"
#include "document/view.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/string.h"
#include "util/time.h"

void add_iframeset_entry(struct iframeset_desc **parent,
		char *url, char *name, int y, int width, int height, int nlink)
{
	struct iframeset_desc *iframeset_desc;
	struct iframe_desc *iframe_desc;
	int offset;
	static char about_blank[] = "about:blank";

	assert(parent);
	if_assert_failed return;

	if (!*parent) {
		*parent = (struct iframeset_desc *)mem_calloc(1, sizeof(struct iframeset_desc));
	} else {
		*parent = (struct iframeset_desc *)mem_realloc(*parent, sizeof(struct iframeset_desc) + ((*parent)->n + 1) * sizeof(struct iframe_desc));
	}
	if (!*parent) return;

	iframeset_desc = *parent;
	offset = iframeset_desc->n;
	iframe_desc = &iframeset_desc->iframe_desc[offset];
	iframe_desc->name = stracpy(name);
	iframe_desc->uri = get_uri(url, URI_NONE);
	iframe_desc->box.x = 1;
	iframe_desc->box.y = y;
	iframe_desc->box.width = width;
	iframe_desc->box.height = height;
	iframe_desc->nlink = nlink;
	if (!iframe_desc->uri)
		iframe_desc->uri = get_uri(about_blank, URI_NONE);

	iframeset_desc->n++;
}

static void
add_iframe_to_list(struct session *ses, struct document_view *doc_view)
{
	struct document_view *ses_doc_view;

	assert(ses && doc_view);
	if_assert_failed return;

	foreach (ses_doc_view, ses->scrn_iframes) {
		int sx = ses_doc_view->box.x;
		int sy = ses_doc_view->box.y;
		int x = doc_view->box.x;
		int y = doc_view->box.y;

		if (sy > y || (sy == y && sx > x)) {
			add_at_pos(ses_doc_view->prev, doc_view);
			return;
		}
	}

	add_to_list_end(ses->scrn_iframes, doc_view);
}

static struct document_view *
find_ifd(struct session *ses, char *name,
	int depth, int x, int y)
{
	struct document_view *doc_view;

	assert(ses && name);
	if_assert_failed return NULL;

	foreachback (doc_view, ses->scrn_iframes) {
		if (doc_view->used) continue;
		if (c_strcasecmp(doc_view->name, name)) continue;

		doc_view->used = 1;
		doc_view->depth = 0;//depth;
		return doc_view;
	}

	doc_view = (struct document_view *)mem_calloc(1, sizeof(*doc_view));
	if (!doc_view) return NULL;

	doc_view->used = 1;
	doc_view->name = stracpy(name);
	if (!doc_view->name) {
		mem_free(doc_view);
		return NULL;
	}
	doc_view->depth = 0;//depth;
	doc_view->session = ses;
	doc_view->search_word = &ses->search_word;
	set_box(&doc_view->box, x, y, 0, 0);

	add_iframe_to_list(ses, doc_view);

	return doc_view;
}


static struct document_view *
format_iframe(struct session *ses, struct iframe_desc *iframe_desc,
	     struct document_options *o, int j)
{
	struct view_state *vs;
	struct document_view *doc_view;
	struct frame *iframe = NULL;
	struct cache_entry *cached;
	int plain;

	assert(ses && iframe_desc && o);
	if_assert_failed return NULL;

	struct location *loc = cur_loc(ses);

	if (!loc) {
		return NULL;
	}

	foreach (iframe, loc->iframes) {
		if (!c_strcasecmp(iframe->name, iframe_desc->name)) break;
	}

	if (!iframe) {
		return NULL;
	}

	vs = &iframe->vs;
	cached = find_in_cache(vs->uri);
	if (!cached) {
		return NULL;
	}

	plain = o->plain;
	if (vs->plain != -1) o->plain = vs->plain;

	if (!cached->redirect || iframe->redirect_cnt >= MAX_REDIRECTS) {
		goto redir;
	}

	iframe->redirect_cnt++;
	done_uri(vs->uri);
	vs->uri = get_uri_reference(cached->redirect);
#ifdef CONFIG_ECMASCRIPT
	vs->ecmascript_fragile = 1;
#endif
	o->plain = plain;
redir:
	doc_view = find_ifd(ses, iframe_desc->name, j, o->box.x, o->box.y);
	if (doc_view) {
		render_document(vs, doc_view, o);
		///assert(doc_view->document);
		//doc_view->document->iframe = frame_desc;
	}
	o->plain = plain;

	return doc_view;
}


void
format_iframes(struct session *ses, struct iframeset_desc *ifsd,
	      struct document_options *op, int depth)
{
	struct document_options o;
	int j;

	assert(ses && ifsd && op);
	if_assert_failed return;

	if (depth > HTML_MAX_FRAME_DEPTH) {
		return;
	}

	copy_struct(&o, op);

	o.margin = !!o.margin;

	for (j = 0; j < ifsd->n; j++) {
		struct iframe_desc *iframe_desc = &ifsd->iframe_desc[j];

		o.box.x = iframe_desc->box.x;
		o.box.y = iframe_desc->box.y;

		o.box.width = iframe_desc->box.width;
		o.box.height = int_min(iframe_desc->box.height, ses->tab->term->height - iframe_desc->box.y - 1);
		o.framename = iframe_desc->name;

		format_iframe(ses, iframe_desc, &o, j);
		o.box.x += o.box.width + 1;
		o.box.y += o.box.height + 1;
#ifdef CONFIG_DEBUG
			/* This makes this ugly loop actually at least remotely
			 * debuggable by gdb, otherwise the compiler happily */
		do_not_optimize_here(&j);
		do_not_optimize_here(&ifsd);
		do_not_optimize_here(&iframe_desc);
#endif
	}
}
