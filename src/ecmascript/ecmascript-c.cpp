/* ECMAScript helper functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dialogs/status.h"
#include "document/document.h"
#include "document/libdom/mapa.h"
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

	if (!doc_view) /* Blank initial document. TODO: Start at about:blank? */
		return;
	assert(doc_view->vs);
	if (doc_view->vs->ecmascript_fragile)
		ecmascript_reset_state(doc_view->vs);
	if (!doc_view->vs->ecmascript)
		return;

	redirect_url = ecmascript_eval_stringback(doc_view->vs->ecmascript,
		&current_url);
	if (!redirect_url)
		return;
	/* XXX: This code snippet is duplicated over here,
	 * location_set_property(), html_a() and who knows where else. */
	redirect_abs_url = join_urls(doc_view->document->uri,
	                             trim_chars(redirect_url, ' ', 0));
	mem_free(redirect_url);
	if (!redirect_abs_url)
		return;
	redirect_uri = get_uri(redirect_abs_url, URI_NONE);
	mem_free(redirect_abs_url);
	if (!redirect_uri)
		return;

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
	if (!vs->ecmascript_fragile)
		assert(vs->ecmascript);
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
			check_for_rerender(vs->ecmascript, "process_snippets");
		}
	}
}

void
kill_ecmascript_timeouts(struct document *document)
{
	struct ecmascript_timeout *t;

	foreach(t, document->timeouts) {
		kill_timer(&t->tid);
		done_string(&t->code);
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
	if (interpreter->vs->doc_view) {
		struct ecmascript_timeout *t;

		foreach (t, interpreter->vs->doc_view->document->timeouts) {
			kill_timer(&t->tid);
			done_string(&t->code);
		}
		free_list(interpreter->vs->doc_view->document->timeouts);
	}
	interpreter->vs->ecmascript = NULL;
	interpreter->vs->ecmascript_fragile = 1;
	mem_free(interpreter);
	--interpreter_count;
}

void
check_events_for_element(struct ecmascript_interpreter *ecmascript, dom_node *element, struct term_event *ev)
{
	const char *event_name = script_event_hook_name[SEVHOOK_ONKEYDOWN];

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
	foreach (fv, vs->forms)
		ecmascript_detach_form_view(fv);
	for (i = 0; i < vs->form_info_len; i++)
		ecmascript_detach_form_state(&vs->form_info[i]);

	vs->ecmascript_fragile = 0;
	if (vs->ecmascript)
		ecmascript_put_interpreter(vs->ecmascript);

	vs->ecmascript = ecmascript_get_interpreter(vs);
	if (!vs->ecmascript)
		vs->ecmascript_fragile = 1;
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
