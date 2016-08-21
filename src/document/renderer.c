/** Generic renderer multiplexer
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "document/document.h"
#include "document/dom/renderer.h"
#include "document/html/frames.h"
#include "document/html/renderer.h"
#include "document/plain/renderer.h"
#include "document/renderer.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "encoding/encoding.h"
#include "intl/charsets.h"
#include "main/main.h"
#include "main/object.h"
#include "protocol/header.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


#ifdef CONFIG_ECMASCRIPT
/** @todo XXX: This function is de facto obsolete, since we do not need to copy
 * snippets around anymore (we process them in one go after the document is
 * loaded; gradual processing was practically impossible because the snippets
 * could reorder randomly during the loading - consider i.e.
 * @<body onLoad>@<script>@</body>: first just @<body> is loaded, but then the
 * rest of the document is loaded and @<script> gets before @<body>; do not even
 * imagine the trouble with rewritten (through scripting hooks) documents;
 * besides, implementing document.write() will be much simpler).
 * But I want to take no risk by reworking that now. --pasky */
static void
add_snippets(struct ecmascript_interpreter *interpreter,
             LIST_OF(struct string_list_item) *doc_snippets,
             LIST_OF(struct string_list_item) *queued_snippets)
{
	struct string_list_item *doc_current = doc_snippets->next;

#ifdef CONFIG_LEDS
	if (list_empty(*queued_snippets) && interpreter->vs->doc_view->session)
		unset_led_value(interpreter->vs->doc_view->session->status.ecmascript_led);
#endif

	if (list_empty(*doc_snippets)
	    || !get_opt_bool("ecmascript.enable", NULL))
		return;

	/* We do this all only once per view_state now. */
	if (!list_empty(*queued_snippets)) {
		/* So if we already did it, we shouldn't need to do it again.
		 * This is the case of moving around in history - we have all
		 * what happenned recorded in the view_state and needn't bother
		 * again. */
#ifdef CONFIG_DEBUG
		/* Hopefully. */
		struct string_list_item *iterator = queued_snippets->next;

		while (iterator != (struct string_list_item *) queued_snippets) {
			if (doc_current == (struct string_list_item *) doc_snippets) {
				INTERNAL("add_snippets(): doc_snippets shorter than queued_snippets!");
				return;
			}
#if 0
			DBG("Comparing snippets\n%.*s\n###### vs #####\n%.*s\n #####",
			    iterator->string.length, iterator->string.source,
			    doc_current->string.length, doc_current->string.source);
#endif
			assert(!strlcmp(iterator->string.source,
			                iterator->string.length,
			                doc_current->string.source,
			                doc_current->string.length));

			doc_current = doc_current->next;
			iterator = iterator->next;
		}
#endif
		return;
	}

	assert(doc_current);
	for (; doc_current != (struct string_list_item *) doc_snippets;
	     doc_current = doc_current->next) {
		add_to_string_list(queued_snippets, doc_current->string.source,
		                   doc_current->string.length);
#if 0
		DBG("Adding snippet\n%.*s\n #####",
		    doc_current->string.length,
		    doc_current->string.source);
#endif
	}
}

static void
process_snippets(struct ecmascript_interpreter *interpreter,
                 LIST_OF(struct string_list_item) *snippets,
                 struct string_list_item **current)
{
	if (!*current)
		*current = snippets->next;
	for (; *current != (struct string_list_item *) snippets;
	     (*current) = (*current)->next) {
		struct string *string = &(*current)->string;
		unsigned char *uristring;
		struct uri *uri;
		struct cache_entry *cached;
		struct fragment *fragment;

		if (string->length == 0)
			continue;

		if (*string->source != '^') {
			/* Evaluate <script>code</script> snippet */
			ecmascript_eval(interpreter, string, NULL);
			continue;
		}

		/* Eval external <script src="reference"></script> snippet */
		uristring = string->source + 1;
		if (!*uristring) continue;

		uri = get_uri(uristring, URI_BASE);
		if (!uri) continue;

		cached = get_redirected_cache_entry(uri);
		done_uri(uri);

		if (!cached) {
			/* At this time (!gradual_rerendering), we should've
			 * already retrieved this though. So it must've been
			 * that it went away because unused and the cache was
			 * already too full. */
#if 0
			/* Disabled because gradual rerendering can be triggered
			 * by numerous events other than a ecmascript reference
			 * completing like the original document and CSS. Problem
			 * is that we should never continue this loop but rather
			 * break out if that is the case. Somehow we need to
			 * be able to derive URI loading problems at this point
			 * or maybe remove reference snippets if they fail to load.
			 *
			 * This FIFO queue handling should be used for also CSS
			 * imports so it would be cool if it could be general
			 * enough for that. Using it for frames with the FIFOing
			 * disabled probably wouldn't hurt either.
			 *
			 * To top this thing off it would be nice if it also
			 * handled dependency tracking between references so that
			 * CSS documents will not disappear from the cache
			 * before all referencing HTML documents has been deleted
			 * from it.
			 *
			 * Reported as bug 533. */
			/* Pasky's explanation: If we get the doc in a single
			 * shot, before calling draw_formatted() we didn't have
			 * anything additional queued for loading and the cache
			 * entry was already loaded, so we didn't get
			 * gradual_loading set. But then while parsing the
			 * document we got some external references and trying
			 * to process them right now. Boom.
			 *
			 * The obvious solution would be to always call
			 * draw_formatted() with gradual_loading in
			 * doc_loading_callback() and if we are sure the
			 * loading is really over, call it one more time
			 * without gradual_loading set. I'm not sure about
			 * the implications though so I won't do it before
			 * 0.10.0. --pasky */
			ERROR("The script of %s was lost in too full a cache!",
			      uristring);
#endif
			continue;
		}

		fragment = get_cache_fragment(cached);
		if (fragment) {
			struct string code = INIT_STRING(fragment->data, fragment->length);

			ecmascript_eval(interpreter, &code, NULL);
		}
	}
}
#endif

static void
render_encoded_document(struct cache_entry *cached, struct document *document)
{
	struct uri *uri = cached->uri;
	enum stream_encoding encoding = ENCODING_NONE;
	struct fragment *fragment = get_cache_fragment(cached);
	struct string buffer = INIT_STRING("", 0);

	/* Even empty documents have to be rendered so that info in the protocol
	 * header, such as refresh info, get processed. (bug 625) */
	if (fragment) {
		buffer.source = fragment->data;
		buffer.length = fragment->length;
	}

	if (uri->protocol != PROTOCOL_FILE) {
		unsigned char *extension = get_extension_from_uri(uri);

		if (extension) {
			encoding = guess_encoding(extension);
			mem_free(extension);
		}

		if (encoding != ENCODING_NONE) {
			int length = 0;
			unsigned char *source;
			struct stream_encoded *stream = open_encoded(-1, encoding);

			if (!stream) {
				encoding = ENCODING_NONE;
			} else {
				source = decode_encoded_buffer(stream, encoding, buffer.source,
					       buffer.length, &length);
				close_encoded(stream);

				if (source) {
					buffer.source = source;
					buffer.length = length;
				} else {
					encoding = ENCODING_NONE;
				}
			}
		}
	}

	if (document->options.plain) {
#ifdef CONFIG_DOM
		if (cached->content_type
		    && (!c_strcasecmp("text/html", cached->content_type)
			|| !c_strcasecmp("application/xhtml+xml", cached->content_type)
		        || !c_strcasecmp("application/docbook+xml", cached->content_type)
		        || !c_strcasecmp("application/rss+xml", cached->content_type)
		        || !c_strcasecmp("application/xbel+xml", cached->content_type)
		        || !c_strcasecmp("application/x-xbel", cached->content_type)
		        || !c_strcasecmp("application/xbel", cached->content_type)))
			render_dom_document(cached, document, &buffer);
		else
#endif
			render_plain_document(cached, document, &buffer);

	} else {
#ifdef CONFIG_DOM
		if (cached->content_type
		    && (!c_strlcasecmp("application/rss+xml", 19, cached->content_type, -1)))
			render_dom_document(cached, document, &buffer);
		else
#endif
			render_html_document(cached, document, &buffer);
	}

	if (encoding != ENCODING_NONE) {
		done_string(&buffer);
	}
}

void
render_document(struct view_state *vs, struct document_view *doc_view,
		struct document_options *options)
{
	unsigned char *name;
	struct document *document;
	struct cache_entry *cached;

	assert(vs && doc_view && options);
	if_assert_failed return;

#if 0
	DBG("(Re%u)Rendering %s on doc_view %p [%s] while attaching it to %p",
	    options->gradual_rerendering, struri(vs->uri),
	    doc_view, doc_view->name, vs);
#endif

	name = doc_view->name;
	doc_view->name = NULL;

	detach_formatted(doc_view);

	doc_view->name = name;
	doc_view->vs = vs;
	doc_view->last_x = doc_view->last_y = -1;

#if 0
	/* This is a nice idea, but doesn't always work: in particular when
	 * there's a frame name conflict. You loaded something to the vs'
	 * frame, but later something tried to get loaded to a frame with
	 * the same name and we got back this frame again, so we are now
	 * overriding the original document with a cuckoo. This assert()ion
	 * should be re-enabled when we start to get this right (which is
	 * very complex, but someone should rewrite the frames support
	 * anyway). --pasky */
	assert(!vs->doc_view);
#else
	if (vs->doc_view) {
		/* It will be still detached, no worries - hopefully it still
		 * resides in ses->scrn_frames. */
		assert(vs->doc_view->vs == vs);
		vs->doc_view->used = 0; /* A bit risky, but... */
		vs->doc_view->vs = NULL;
		vs->doc_view = NULL;
#ifdef CONFIG_ECMASCRIPT
		vs->ecmascript_fragile = 1; /* And is this good? ;-) */
#endif
	}
#endif
	vs->doc_view = doc_view;

	cached = find_in_cache(vs->uri);
	if (!cached) {
		INTERNAL("document %s to format not found", struri(vs->uri));
		return;
	}

	document = get_cached_document(cached, options);
	if (document) {
		doc_view->document = document;
	} else {
		document = init_document(cached, options);
		if (!document) return;
		doc_view->document = document;

		if (doc_view->session
		    && doc_view->session->reloadlevel > CACHE_MODE_NORMAL)
			for (; vs->form_info_len > 0; vs->form_info_len--)
				done_form_state(&vs->form_info[vs->form_info_len - 1]);

		shrink_memory(0);

		render_encoded_document(cached, document);
		sort_links(document);
		if (!document->title) {
			enum uri_component components;

			if (document->uri->protocol == PROTOCOL_FILE) {
				components = URI_PATH;
			} else {
				components = URI_PUBLIC;
			}

			document->title = get_uri_string(document->uri, components);
			if (document->title) {
#ifdef CONFIG_UTF8
				if (doc_view->document->options.utf8)
					decode_uri(document->title);
				else
#endif /* CONFIG_UTF8 */
					decode_uri_for_display(document->title);
			}
		}

#ifdef CONFIG_CSS
		document->css_magic = get_document_css_magic(document);
#endif
	}
#ifdef CONFIG_ECMASCRIPT
	if (!vs->ecmascript_fragile)
		assert(vs->ecmascript);
	if (!options->gradual_rerendering) {
		/* We also reset the state if the underlying document changed
		 * from the last time we did the snippets. This may be
		 * triggered i.e. when redrawing a document which has been
		 * reloaded in a different tab meanwhile (OTOH we don't want
		 * to reset the state if we are redrawing a document we have
		 * already drawn before).
		 *
		 * (vs->ecmascript->onload_snippets_owner) check may be
		 * superfluous since we should always have
		 * vs->ecmascript_fragile set in those cases; that's why we
		 * don't ever bother to re-zero it if we are suddenly doing
		 * gradual rendering again.
		 *
		 * XXX: What happens if a document is still loading in the
		 * other tab when we press ^L here? */
		if (vs->ecmascript_fragile
		    || (vs->ecmascript
		       && vs->ecmascript->onload_snippets_cache_id
		       && document->cache_id != vs->ecmascript->onload_snippets_cache_id))
			ecmascript_reset_state(vs);
		/* If ecmascript_reset_state cannot construct a new
		 * ECMAScript interpreter, it sets vs->ecmascript =
		 * NULL and vs->ecmascript_fragile = 1.  */
		if (vs->ecmascript) {
			vs->ecmascript->onload_snippets_cache_id = document->cache_id;

			/* Passing of the onload_snippets pointers
			 * gives *_snippets() some feeling of
			 * universality, shall we ever get any other
			 * snippets (?). */
			add_snippets(vs->ecmascript,
				     &document->onload_snippets,
				     &vs->ecmascript->onload_snippets);
			process_snippets(vs->ecmascript,
					 &vs->ecmascript->onload_snippets,
					 &vs->ecmascript->current_onload_snippet);
		}
	}
#endif

	/* If we do not care about the height and width of the document
	 * just use the setup values. */

	copy_box(&doc_view->box, &document->options.box);

	if (!document->options.needs_width)
		doc_view->box.width = options->box.width;

	if (!document->options.needs_height)
		doc_view->box.height = options->box.height;
}


void
render_document_frames(struct session *ses, int no_cache)
{
	struct document_options doc_opts;
	struct document_view *doc_view;
	struct document_view *current_doc_view = NULL;
	struct view_state *vs = NULL;

	if (!ses->doc_view) {
		ses->doc_view = mem_calloc(1, sizeof(*ses->doc_view));
		if (!ses->doc_view) return;
		ses->doc_view->session = ses;
		ses->doc_view->search_word = &ses->search_word;
	}

	if (have_location(ses)) vs = &cur_loc(ses)->vs;

	init_document_options(ses, &doc_opts);

	set_box(&doc_opts.box, 0, 0,
		ses->tab->term->width, ses->tab->term->height);

	if (ses->status.show_title_bar) {
		doc_opts.box.y++;
		doc_opts.box.height--;
	}
	if (ses->status.show_status_bar) doc_opts.box.height--;
	if (ses->status.show_tabs_bar) {
		doc_opts.box.height--;
		if (ses->status.show_tabs_bar_at_top) doc_opts.box.y++;
	}

	doc_opts.color_mode = get_opt_int_tree(ses->tab->term->spec, "colors",
	                                       NULL);
	if (!get_opt_bool_tree(ses->tab->term->spec, "underline", NULL))
		doc_opts.color_flags |= COLOR_ENHANCE_UNDERLINE;

	doc_opts.cp = get_terminal_codepage(ses->tab->term);
	doc_opts.no_cache = no_cache & 1;
	doc_opts.gradual_rerendering = !!(no_cache & 2);

	if (vs) {
		if (vs->plain < 0) vs->plain = 0;
		doc_opts.plain = vs->plain;
		doc_opts.wrap = vs->wrap;
	} else {
		doc_opts.plain = 1;
	}

	foreach (doc_view, ses->scrn_frames) doc_view->used = 0;

	if (vs) render_document(vs, ses->doc_view, &doc_opts);

	if (document_has_frames(ses->doc_view->document)) {
		current_doc_view = current_frame(ses);
		format_frames(ses, ses->doc_view->document->frame_desc, &doc_opts, 0);
	}

	foreach (doc_view, ses->scrn_frames) {
		struct document_view *prev_doc_view = doc_view->prev;

		if (doc_view->used) continue;

		detach_formatted(doc_view);
		del_from_list(doc_view);
		mem_free(doc_view);
		doc_view = prev_doc_view;
	}

	if (current_doc_view) {
		int n = 0;

		foreach (doc_view, ses->scrn_frames) {
			if (document_has_frames(doc_view->document)) continue;
			if (doc_view == current_doc_view) {
				cur_loc(ses)->vs.current_link = n;
				break;
			}
			n++;
		}
	}
}

/* comparison function for qsort() */
static int
comp_links(const void *v1, const void *v2)
{
	const struct link *l1 = v1, *l2 = v2;

	assert(l1 && l2);
	if_assert_failed return 0;
	return (l1->number - l2->number);
}

void
sort_links(struct document *document)
{
	int i;

	assert(document);
	if_assert_failed return;
	if (!document->nlinks) return;

	if (document->links_sorted) return;
	assert(document->links);
	if_assert_failed return;

	qsort(document->links, document->nlinks, sizeof(*document->links),
	      comp_links);

	if (!document->height) return;

	mem_free_if(document->lines1);
	document->lines1 = mem_calloc(document->height, sizeof(*document->lines1));
	mem_free_if(document->lines2);
	if (!document->lines1) return;
	document->lines2 = mem_calloc(document->height, sizeof(*document->lines2));
	if (!document->lines2) {
		mem_free(document->lines1);
		return;
	}

	for (i = 0; i < document->nlinks; i++) {
		struct link *link = &document->links[i];
		int p, q, j;

		if (!link->npoints) {
			done_link_members(link);
			memmove(link, link + 1,
				(document->nlinks - i - 1) * sizeof(*link));
			document->nlinks--;
			i--;
			continue;
		}
		p = link->points[0].y;
		q = link->points[link->npoints - 1].y;
		if (p > q) j = p, p = q, q = j;
		for (j = p; j <= q; j++) {
			assertm(j < document->height, "link out of screen");
			if_assert_failed continue;
			document->lines2[j] = &document->links[i];
			if (!document->lines1[j])
				document->lines1[j] = &document->links[i];
		}
	}
	document->links_sorted = 1;
}

struct conv_table *
get_convert_table(unsigned char *head, int to_cp,
		  int default_cp, int *from_cp,
		  enum cp_status *cp_status, int ignore_server_cp)
{
	unsigned char *part = head;
	int cp_index = -1;

	assert(head);
	if_assert_failed return NULL;

	if (ignore_server_cp) {
		if (cp_status) *cp_status = CP_STATUS_IGNORED;
		if (from_cp) *from_cp = default_cp;
		return get_translation_table(default_cp, to_cp);
	}

	while (cp_index == -1) {
		unsigned char *ct_charset;
		/* scan_http_equiv() appends the meta http-equiv directives to
		 * the protocol header before this function is called, but the
		 * HTTP Content-Type header has precedence, so the HTTP header
		 * will be used if it exists and the meta header is only used
		 * as a fallback.  See bug 983.  */
		unsigned char *a = parse_header(part, "Content-Type", &part);

		if (!a) break;

		parse_header_param(a, "charset", &ct_charset);
		if (ct_charset) {
			cp_index = get_cp_index(ct_charset);
			mem_free(ct_charset);
		}
		mem_free(a);
	}

	if (cp_index == -1) {
		unsigned char *a = parse_header(head, "Content-Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_index == -1) {
		unsigned char *a = parse_header(head, "Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_index == -1) {
		cp_index = default_cp;
		if (cp_status) *cp_status = CP_STATUS_ASSUMED;
	} else {
		if (cp_status) *cp_status = CP_STATUS_SERVER;
	}

	if (from_cp) *from_cp = cp_index;

	return get_translation_table(cp_index, to_cp);
}
