/* ECMAScript helper functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "ecmascript/libdom/dom.h"

#include "dialogs/status.h"
#include "document/document.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#ifdef CONFIG_MUJS
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/element.h"
#endif
#ifdef CONFIG_QUICKJS
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/element.h"
#endif
#ifdef CONFIG_ECMASCRIPT_SMJS
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/element.h"
#endif
#include "intl/libintl.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/event.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/view.h"

extern int interpreter_count;
extern int ecmascript_enabled;

int
ecmascript_get_interpreter_count(void)
{
	return interpreter_count;
}

void
toggle_ecmascript(struct session *ses)
{
	ecmascript_enabled = !ecmascript_enabled;

	if (ecmascript_enabled) {
		mem_free_set(&ses->status.window_status, stracpy(_("Ecmascript enabled", ses->tab->term)));
	} else {
		mem_free_set(&ses->status.window_status, stracpy(_("Ecmascript disabled", ses->tab->term)));
	}
	print_screen_status(ses);
}

void
ecmascript_protocol_handler(struct session *ses, struct uri *uri)
{
	struct document_view *doc_view = current_frame(ses);
	struct string current_url = INIT_STRING(struri(uri), (int)strlen(struri(uri)));
	char *redirect_url, *redirect_abs_url;
	struct uri *redirect_uri;

	if (!doc_view) {/* Blank initial document. TODO: Start at about:blank? */
		return;
	}
	assert(doc_view->vs);
	if (doc_view->vs->ecmascript_fragile) {
		ecmascript_reset_state(doc_view->vs);
	}
	if (!doc_view->vs->ecmascript) {
		return;
	}

	redirect_url = ecmascript_eval_stringback(doc_view->vs->ecmascript,
		&current_url);
	if (!redirect_url) {
		return;
	}
	/* XXX: This code snippet is duplicated over here,
	 * location_set_property(), html_a() and who knows where else. */
	redirect_abs_url = join_urls(doc_view->document->uri,
	                             trim_chars(redirect_url, ' ', 0));
	mem_free(redirect_url);
	if (!redirect_abs_url) {
		return;
	}
	redirect_uri = get_uri(redirect_abs_url, URI_NONE);
	mem_free(redirect_abs_url);
	if (!redirect_uri) {
		return;
	}

	/* XXX: Is that safe to do at this point? --pasky */
	goto_uri_frame(ses, redirect_uri, doc_view->name,
		CACHE_MODE_NORMAL);
	done_uri(redirect_uri);
}

static void
add_snippets(struct ecmascript_interpreter *interpreter,
             LIST_OF(struct ecmascript_string_list_item) *doc_snippets,
             LIST_OF(struct ecmascript_string_list_item) *queued_snippets)
{
	struct ecmascript_string_list_item *doc_current = (struct ecmascript_string_list_item *)doc_snippets->next;

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
		struct ecmascript_string_list_item *iterator = queued_snippets->next;

		while (iterator != (struct ecmascript_string_list_item *) queued_snippets) {
			if (doc_current == (struct ecmascript_string_list_item *) doc_snippets) {
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
	for (; doc_current != (struct ecmascript_string_list_item *) doc_snippets;
	     doc_current = doc_current->next) {
		add_to_ecmascript_string_list(queued_snippets, doc_current->string.source,
		                   doc_current->string.length, doc_current->element_offset);
#if 0
		DBG("Adding snippet\n%.*s\n #####",
		    doc_current->string.length,
		    doc_current->string.source);
#endif
	}
}

static void
process_snippets(struct ecmascript_interpreter *interpreter,
                 LIST_OF(struct ecmascript_string_list_item) *snippets,
                 struct ecmascript_string_list_item **current)
{
	if (!*current)
		*current = (struct ecmascript_string_list_item *)snippets->next;
	for (; *current != (struct ecmascript_string_list_item *) snippets;
	     (*current) = (*current)->next) {
		struct string *string = &(*current)->string;
		char *uristring;
		struct uri *uri;
		struct cache_entry *cached;
		struct fragment *fragment;

		if (string->length == 0)
			continue;

		if (*string->source != '^') {
			/* Evaluate <script>code</script> snippet */
			ecmascript_eval(interpreter, string, NULL, (*current)->element_offset);
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
			struct string code = INIT_STRING(fragment->data, (int)fragment->length);

			ecmascript_eval(interpreter, &code, NULL, (*current)->element_offset);
		}
	}
	check_for_rerender(interpreter, "eval");
}

void
check_for_snippets(struct view_state *vs, struct document_options *options, struct document *document)
{
	if (!vs->ecmascript_fragile) {
		assert(vs->ecmascript);
	}

	if (!options->dump && !options->gradual_rerendering) {
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
		       && vs->ecmascript->onload_snippets_cache_id)) {
			ecmascript_reset_state(vs);
		}
		/* If ecmascript_reset_state cannot construct a new
		 * ECMAScript interpreter, it sets vs->ecmascript =
		 * NULL and vs->ecmascript_fragile = 1.  */
		if (vs->ecmascript) {

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

			if (!vs->plain) {
				fire_onload(document->dom);
			}
			check_for_rerender(vs->ecmascript, "process_snippets");

			vs->ecmascript->onload_snippets_cache_id = 0;
		}
	}
}

void
ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	assert(interpreter->backend_nesting == 0);
	/* If the assertion fails, it is better to leak the
	 * interpreter than to corrupt memory.  */
	if_assert_failed return;
#ifdef CONFIG_MUJS
	mujs_put_interpreter(interpreter);
#elif defined(CONFIG_QUICKJS)
	quickjs_put_interpreter(interpreter);
#else
	spidermonkey_put_interpreter(interpreter);
#endif
	free_ecmascript_string_list(&interpreter->onload_snippets);
	done_string(&interpreter->code);
	free_ecmascript_string_list(&interpreter->writecode);
	/* Is it superfluous? */
	if (1) {
		struct ecmascript_timeout *t;

		foreach (t, interpreter->timeouts) {
			kill_timer(&t->tid);
			done_string(&t->code);
#ifdef CONFIG_ECMASCRIPT_SMJS
			delete(t->fun);
#endif
#ifdef CONFIG_QUICKJS
			if (!JS_IsNull(t->fun)) {
				JS_FreeValue(t->ctx, t->fun);
			}
#endif
		}
		free_list(interpreter->timeouts);
#ifdef CONFIG_QUICKJS
		if (!JS_IsNull(interpreter->location_obj)) {
			//JS_FreeValue(t->ctx, interpreter->location_obj);
		}
		if (!JS_IsNull(interpreter->document_obj)) {
			//JS_FreeValue(t->ctx, interpreter->document_obj);
		}
#endif
	}
#ifdef CONFIG_ECMASCRIPT_SMJS
	//js::StopDrainingJobQueue((JSContext *)interpreter->backend_data);
#endif
	interpreter->vs->ecmascript = NULL;
	interpreter->vs->ecmascript_fragile = 1;
	mem_free(interpreter);
	--interpreter_count;
}

void
check_events_for_element(struct ecmascript_interpreter *ecmascript, dom_node *element, struct term_event *ev)
{
	const char *event_name = script_event_hook_name[SEVHOOK_ONKEYDOWN];

	if (!ecmascript) {
		return;
	}

	check_element_event(ecmascript, element, event_name, ev);
	event_name = script_event_hook_name[SEVHOOK_ONKEYUP];
	check_element_event(ecmascript, element, event_name, ev);
	event_name = script_event_hook_name[SEVHOOK_ONKEYPRESS];
	check_element_event(ecmascript, element, event_name, ev);
}

void
ecmascript_reset_state(struct view_state *vs)
{
	struct form_view *fv;
	int i;

	/* Normally, if vs->ecmascript == NULL, the associated
	 * ecmascript_obj pointers are also NULL.  However, they might
	 * be non-NULL if the ECMAScript objects have been lazily
	 * created because of scripts running in sibling HTML frames.  */
	foreach (fv, vs->forms) {
		ecmascript_detach_form_view(fv);
	}
	for (i = 0; i < vs->form_info_len; i++) {
		ecmascript_detach_form_state(&vs->form_info[i]);
	}

	vs->ecmascript_fragile = 0;
	if (vs->ecmascript) {
		ecmascript_put_interpreter(vs->ecmascript);
	}

	vs->ecmascript = ecmascript_get_interpreter(vs);
	if (!vs->ecmascript) {
		vs->ecmascript_fragile = 1;
	}
}

int
ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter,
			 struct string *code)
{
	int result;

	if (!get_ecmascript_enable(interpreter))
		return -1;
	assert(interpreter);
	interpreter->backend_nesting++;
#ifdef CONFIG_MUJS
	result = mujs_eval_boolback(interpreter, code);
#elif defined(CONFIG_QUICKJS)
	result = quickjs_eval_boolback(interpreter, code);
#else
	result = spidermonkey_eval_boolback(interpreter, code);
#endif
	interpreter->backend_nesting--;

	check_for_rerender(interpreter, "boolback");

	return result;
}

int
ecmascript_current_link_evhook(struct document_view *doc_view, enum script_event_hook_type type)
{
	struct link *link;
	struct script_event_hook *evhook;

	assert(doc_view && doc_view->vs);
	link = get_current_link(doc_view);
	if (!link) return -1;
	if (!doc_view->vs->ecmascript) return -1;

	void *mapa = (void *)doc_view->document->element_map;

	if (mapa) {
		dom_node *elem = (dom_node *)find_in_map(mapa, link->element_offset);

		if (elem) {
			const char *event_name = script_event_hook_name[(int)type];
			check_element_event(doc_view->vs->ecmascript, elem, event_name, NULL);
		}
	}

	if (!link->event_hooks) return -1;

	foreach (evhook, *link->event_hooks) {
		char *ret;

		if (evhook->type != type) continue;
		ret = evhook->src;
		while ((ret = strstr(ret, "return ")))
			while (*ret != ' ') *ret++ = ' ';
		{
			struct string src = INIT_STRING(evhook->src, (int)strlen(evhook->src));
			/* TODO: Some even handlers return a bool. */
			if (!ecmascript_eval_boolback(doc_view->vs->ecmascript, &src))
				return 0;
		}
	}

	return 1;
}

void
ecmascript_detach_form_view(struct form_view *fv)
{
#ifdef CONFIG_MUJS
#elif defined(CONFIG_QUICKJS)
	quickjs_detach_form_view(fv);
#else
	spidermonkey_detach_form_view(fv);
#endif
}

void ecmascript_detach_form_state(struct form_state *fs)
{
#ifdef CONFIG_MUJS
#elif defined(CONFIG_QUICKJS)
	quickjs_detach_form_state(fs);
#else
	spidermonkey_detach_form_state(fs);
#endif
}

void ecmascript_moved_form_state(struct form_state *fs)
{
#ifdef CONFIG_MUJS
#elif defined(CONFIG_QUICKJS)
	quickjs_moved_form_state(fs);
#else
	spidermonkey_moved_form_state(fs);
#endif
}

void *
walk_tree_query(dom_node *node, const char *selector, int depth)
{
	dom_exception exc;
	dom_node *child = NULL;
	void *res = NULL;
	dom_node_type typ;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &typ);
	if (typ != DOM_ELEMENT_NODE) {
		return NULL;
	}

	if ((res = el_match_selector(selector, node))) {
		/* There was an error; return */
		return res;
	}
	/* Get the node's first child */
	exc = dom_node_get_first_child(node, &child);

	if (exc != DOM_NO_ERR) {
		return NULL;
	} else if (child != NULL) {
		/* node has children;  decend to children's depth */
		depth++;

		/* Loop though all node's children */
		do {
			dom_node *next_child = NULL;

			/* Visit node's descendents */
			res = walk_tree_query(child, selector, depth);
			/* There was an error; return */
			if (res) {
				dom_node_unref(child);
				return res;
			}

			/* Go to next sibling */
			exc = dom_node_get_next_sibling(child, &next_child);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(child);
				return NULL;
			}

			dom_node_unref(child);
			child = next_child;
		} while (child != NULL); /* No more children */
	}
	return NULL;
}

void
walk_tree_query_append(dom_node *node, const char *selector, int depth, LIST_OF(struct selector_node) *result_list)
{
	dom_exception exc;
	dom_node *child;
	void *res = NULL;
	dom_node_type typ;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &typ);

	if (typ != DOM_ELEMENT_NODE) {
		return;
	}

	if ((res = el_match_selector(selector, node))) {
		struct selector_node *sn = (struct selector_node *)mem_calloc(1, sizeof(*sn));

		if (sn) {
			sn->node = res;
			add_to_list_end(*result_list, sn);
		}
	}
	/* Get the node's first child */
	exc = dom_node_get_first_child(node, &child);

	if (exc != DOM_NO_ERR) {
		return;
	} else if (child != NULL) {
		/* node has children;  decend to children's depth */
		depth++;

		/* Loop though all node's children */
		do {
			dom_node *next_child;

			/* Visit node's descendents */
			walk_tree_query_append(child, selector, depth, result_list);

			/* Go to next sibling */
			exc = dom_node_get_next_sibling(child, &next_child);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(child);
				return;
			}

			dom_node_unref(child);
			child = next_child;
		} while (child != NULL); /* No more children */
	}
}

static bool
node_has_classes(struct dom_node *node, void *ctx)
{
	LIST_OF(struct class_string) *list = (LIST_OF(struct class_string) *)ctx;
	struct class_string *st;

	if (list_empty(*list)) {
		return false;
	}

	foreach (st, *list) {
		bool ret = false;
		dom_exception exc = dom_element_has_class((struct dom_element *)node, st->name, &ret);

		if (exc != DOM_NO_ERR || !ret) {
			return false;
		}
	}

	return true;
}

static LIST_OF(struct class_string) *
prepare_strings(const char *text)
{
	if (!text) {
		return NULL;
	}
	LIST_OF(struct class_string) *list = (LIST_OF(struct class_string) *)mem_calloc(1, sizeof(*list));

	if (!list) {
		return NULL;
	}
	init_list(*list);
	char *pos = (char *)text;

	while (*pos) {
		skip_space(pos);
		if (*pos) {
			char *begin = pos;

			skip_nonspace(pos);
			struct class_string *klass = (struct class_string *)mem_calloc(1, sizeof(*klass));

			if (!klass) {
				continue;
			}
			lwc_string *ls = NULL;
			lwc_error err = lwc_intern_string(begin, pos - begin, &ls);

			if (err == lwc_error_ok) {
				klass->name = ls;
				add_to_list_end(*list, klass);
			} else {
				mem_free(klass);
			}
		}
	}

	return list;
}

void
free_el_dom_collection(void *ctx)
{
	if (!ctx) {
		return;
	}
	LIST_OF(struct class_string) *list = (LIST_OF(struct class_string) *)ctx;
	struct class_string *st;

	foreach (st, *list) {
		lwc_string *ls = (lwc_string *)st->name;

		if (ls) {
			lwc_string_unref(ls);
		}
	}
	free_list(*list);
	mem_free(list);
}


#if 0
typedef bool (*dom_callback_is_in_collection)(struct dom_node *node, void *ctx);

/**
 * The html_collection structure
 */
struct el_dom_html_collection {
	dom_callback_is_in_collection ic;
	/**< The function pointer used to test
	 * whether some node is an element of
	 * this collection
	 */
	void *ctx; /**< Context for the callback */
	struct dom_html_document *doc;  /**< The document created this
                                     * collection
                                     */
	dom_node *root; /**< The root node of this collection */
	uint32_t refcnt; /**< Reference counting */
};
#endif

/**
 * Intialiase a dom_html_collection
 *
 * \param doc   The document
 * \param col   The collection object to be initialised
 * \param root  The root element of the collection
 * \param ic    The callback function used to determin whether certain node
 *              beint32_ts to the collection
 * \return DOM_NO_ERR on success.
 */
static dom_exception
el_dom_html_collection_initialise(dom_html_document *doc,
                struct el_dom_html_collection *col,
                dom_node *root,
                dom_callback_is_in_collection ic, void *ctx)
{
	assert(doc != NULL);
	assert(ic != NULL);
	assert(root != NULL);

	col->doc = doc;
	dom_node_ref(doc);
	col->root = root;
	dom_node_ref(root);

	col->ic = ic;
	col->ctx = ctx;
	col->refcnt = 1;

	return DOM_NO_ERR;
}

static dom_exception
el_dom_html_collection_create(dom_html_document *doc,
                dom_node *root,
                dom_callback_is_in_collection ic,
                void *ctx,
                struct el_dom_html_collection **col)
{
	*col = (struct el_dom_html_collection *)malloc(sizeof(struct el_dom_html_collection));

	if (*col == NULL) {
		return DOM_NO_MEM_ERR;
	}

	return el_dom_html_collection_initialise(doc, *col, root, ic, ctx);
}

void *
get_elements_by_class_name(dom_html_document *doc, dom_node *node, const char *classes)
{
	if (!node || !classes) {
		return NULL;
	}
	LIST_OF(struct class_string) *list = prepare_strings(classes);

	if (!list) {
		return NULL;
	}
	struct el_dom_html_collection *col = NULL;
	dom_exception exc = el_dom_html_collection_create(doc, node, node_has_classes, list, &col);

	if (exc != DOM_NO_ERR || !col) {
		return NULL;
	}

	return col;
}

void
camel_to_html(const char *text, struct string *result)
{
	add_to_string(result, "data-");

	for (; *text; text++) {
		if (*text >= 'A' && *text <= 'Z') {
			add_char_to_string(result, '-');
			add_char_to_string(result, *text + 32);
		} else {
			add_char_to_string(result, *text);
		}
	}
}

static bool
el_dump_node_element_attribute(struct string *buf, dom_node *node)
{
	dom_exception exc;
	dom_string *attr = NULL;
	dom_string *attr_value = NULL;

	exc = dom_attr_get_name((struct dom_attr *)node, &attr);

	if (exc != DOM_NO_ERR) {
//		fprintf(stderr, "Exception raised for dom_string_create\n");
		return false;
	}

	/* Get attribute's value */
	exc = dom_attr_get_value((struct dom_attr *)node, &attr_value);
	if (exc != DOM_NO_ERR) {
//		fprintf(stderr, "Exception raised for element_get_attribute\n");
		dom_string_unref(attr);
		return false;
	} else if (attr_value == NULL) {
		/* Element lacks required attribute */
		dom_string_unref(attr);
		return true;
	}

	add_char_to_string(buf, ' ');
	add_bytes_to_string(buf, dom_string_data(attr), dom_string_byte_length(attr));
	add_to_string(buf, "=\"");
	add_bytes_to_string(buf, dom_string_data(attr_value), dom_string_byte_length(attr_value));
	add_char_to_string(buf, '"');

	/* Finished with the attr dom_string */
	dom_string_unref(attr);
	dom_string_unref(attr_value);

	return true;
}


static bool
el_dump_element(struct string *buf, dom_node *node, bool toSortAttrs)
{
// TODO toSortAttrs
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	dom_namednodemap *attrs = NULL;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
//		fprintf(stderr, "Exception raised for node_get_node_type\n");
		return false;
	} else {
		if (type == DOM_TEXT_NODE) {
			dom_string *str;

			exc = dom_node_get_text_content(node, &str);

			if (exc == DOM_NO_ERR && str != NULL) {
				int length = dom_string_byte_length(str);
				const char *string_text = dom_string_data(str);

				if (!((length == 1) && (*string_text == '\n'))) {
					add_bytes_to_string(buf, string_text, length);
				}
				dom_string_unref(str);
			}
			return true;
		}
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}

	/* Get element name */
	exc = dom_node_get_node_name(node, &node_name);
	if (exc != DOM_NO_ERR) {
//		fprintf(stderr, "Exception raised for get_node_name\n");
		return false;
	}

	add_char_to_string(buf, '<');
	//save_in_map(mapa, node, buf->length);

	/* Get string data and print element name */
	add_lowercase_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));

	exc = dom_node_get_attributes(node, &attrs);

	if (exc == DOM_NO_ERR) {
		dom_ulong length;

		exc = dom_namednodemap_get_length(attrs, &length);

		if (exc == DOM_NO_ERR) {
			int i;

			for (i = 0; i < length; ++i) {
				dom_node *attr;

				exc = dom_namednodemap_item(attrs, i, &attr);

				if (exc == DOM_NO_ERR) {
					el_dump_node_element_attribute(buf, attr);
					dom_node_unref(attr);
				}
			}
		}
	}
	if (attrs) {
		dom_namednodemap_unref(attrs);
	}
	add_char_to_string(buf, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

void
ecmascript_walk_tree(struct string *buf, void *nod, bool start, bool toSortAttrs)
{
	dom_node *node = (dom_node *)(nod);
	dom_nodelist *children = NULL;
	dom_node_type type;
	dom_exception exc;
	uint32_t size = 0;

	if (!start) {
		exc = dom_node_get_node_type(node, &type);

		if (exc == DOM_NO_ERR && type == DOM_TEXT_NODE) {
			dom_string *content = NULL;
			exc = dom_node_get_text_content(node, &content);

			if (exc == DOM_NO_ERR && content) {
				add_bytes_to_string(buf, dom_string_data(content), dom_string_length(content));
				dom_string_unref(content);
			}
		} else if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			el_dump_element(buf, node, toSortAttrs);
		}
	}
	exc = dom_node_get_child_nodes(node, &children);

	if (exc == DOM_NO_ERR && children) {
		exc = dom_nodelist_get_length(children, &size);
		uint32_t i;

		for (i = 0; i < size; i++) {
			dom_node *item = NULL;
			exc = dom_nodelist_item(children, i, &item);

			if (exc == DOM_NO_ERR && item) {
				ecmascript_walk_tree(buf, item, false, toSortAttrs);
				dom_node_unref(item);
			}
		}
		dom_nodelist_unref(children);
	}

	if (!start) {
		exc = dom_node_get_node_type(node, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_string *node_name = NULL;
			exc = dom_node_get_node_name(node, &node_name);

			if (exc == DOM_NO_ERR && node_name) {
				add_to_string(buf, "</");
				add_lowercase_to_string(buf, dom_string_data(node_name), dom_string_length(node_name));
				add_char_to_string(buf, '>');
				dom_string_unref(node_name);
			}
		}
	}
}
