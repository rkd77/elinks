#ifndef EL__JS_ECMASCRIPT_H
#define EL__JS_ECMASCRIPT_H

/* This is a trivial ECMAScript driver. All your base are belong to pasky. */
/* In the future you will get DOM, a complete ECMAScript interface and free
 * plasm displays for everyone. */

#include <stdbool.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_ECMASCRIPT_SMJS
#include <jsapi.h>
#include <jsfriendapi.h>
#endif

#ifdef CONFIG_QUICKJS
#include <quickjs/quickjs.h>
#endif

#ifdef CONFIG_MUJS
#include <mujs.h>
#endif

#ifdef CONFIG_ECMASCRIPT

#include "main/module.h"
#include "main/timer.h"
#include "util/string.h"
#include "util/time.h"

#ifdef ECMASCRIPT_DEBUG
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct document_view;
struct ecmascript_timeout;
struct form_state;
struct form_view;
struct string;
struct terminal;
struct uri;
struct view_state;

struct ecmascript_interpreter {
	struct view_state *vs;
	void *backend_data;

	/* Nesting level of calls to backend functions.  When this is
	 * nonzero, there are references to backend_data in the C
	 * stack, so it is not safe to free the data yet.  */
	int backend_nesting;

	/* Used by document.write() */
	struct string *ret;

	/* The code evaluated by setTimeout() */
	struct string code;

	/* document.write buffer */
	LIST_OF(struct ecmascript_string_list_item) writecode;
	struct ecmascript_string_list_item *current_writecode;

	struct heartbeat *heartbeat;

	/* This is a cross-rerenderings accumulator of
	 * @document.onload_snippets (see its description for juicy details).
	 * They enter this list as they continue to appear there, and they
	 * never leave it (so that we can always find from where to look for
	 * any new snippets in document.onload_snippets). Instead, as we
	 * go through the list we maintain a pointer to the last processed
	 * entry. */
	LIST_OF(struct ecmascript_string_list_item) onload_snippets;
	struct ecmascript_string_list_item *current_onload_snippet;

	/* ID of the {struct document} where those onload_snippets belong to.
	 * It is kept at 0 until it is definitively hard-attached to a given
	 * final document. Then if we suddenly appear with this structure upon
	 * a document with a different ID, we reset the state and start with a
	 * fresh one (normally, that does not happen since reloading sets
	 * ecmascript_fragile, but it can happen i.e. when the urrent document
	 * is reloaded in another tab and then you just cause the current tab
	 * to redraw. */
	unsigned int onload_snippets_cache_id;

	/** used by setTimeout */
	LIST_OF(struct ecmascript_timeout) timeouts;
#ifdef CONFIG_ECMASCRIPT_SMJS
	JSAutoRealm *ar;
	JS::Heap<JSObject*> *ac;
	JSObject *document_obj;
	JSObject *location_obj;
	JS::Heap<JS::Value> *fun;
#endif
#ifdef CONFIG_QUICKJS
	JSRuntime *rt;
	JSValue document_obj;
	JSValue location_obj;
	JSValueConst fun;
#endif
#ifdef CONFIG_MUJS
	void *document_obj;
	void *location_obj;
	const char *fun;
	void *doc;
#endif
	int element_offset;
	unsigned int changed:1;
	unsigned int was_write:1;
};

struct ecmascript_timeout {
	LIST_HEAD_EL(struct ecmascript_timeout);
	struct string code;
#ifdef CONFIG_QUICKJS
	JSContext *ctx;
	JSValueConst fun;
#endif
#ifdef CONFIG_ECMASCRIPT_SMJS
	JSContext *ctx;
	JS::Heap<JS::Value> *fun;
#endif
#ifdef CONFIG_MUJS
	js_State *ctx;
	const char *fun;
#endif
	struct ecmascript_interpreter *interpreter;
	timer_id_T tid;
	int timeout_next;
};

struct delayed_goto {
	/* It might look more convenient to pass doc_view around but it could
	 * disappear during wild dances inside of frames or so. */
	struct view_state *vs;
	struct uri *uri;
	unsigned int reload:1;
};

/* Why is the interpreter bound to {struct view_state} instead of {struct
 * document}? That's easy, because the script won't raid just inside of the
 * document, but it will also want to generate pop-up boxes, adjust form
 * contents (which is doc_view-specific) etc. Of course the cons are that we
 * need to wait with any javascript code execution until we get bound to the
 * view_state through document_view - that means we are going to re-render the
 * document if it contains a <script> area full of document.write()s. And why
 * not bound the interpreter to {struct document_view} then? Because it is
 * reset for each rerendering, and it sucks to do all the magic to preserve the
 * interpreter over the rerenderings (we tried). */

int ecmascript_check_url(char *url, char *frame);
void ecmascript_free_urls(struct module *module);

struct ecmascript_interpreter *ecmascript_get_interpreter(struct view_state*vs);

void ecmascript_reset_state(struct view_state *vs);

void ecmascript_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret, int element_offset);
char *ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);

void ecmascript_timeout_dialog(struct terminal *term, int max_exec_time);

void ecmascript_set_action(char **action, char *string);

struct ecmascript_timeout *ecmascript_set_timeout(void *ctx, char *code, int timeout, int timeout_next);

#ifdef CONFIG_ECMASCRIPT_SMJS
struct ecmascript_timeout *ecmascript_set_timeout2(void *ctx, JS::HandleValue f, int timeout, int timeout_next);
#endif

#ifdef CONFIG_QUICKJS
struct ecmascript_timeout *ecmascript_set_timeout2q(void *ctx, JSValue f, int timeout, int timeout_next);
#endif

#ifdef CONFIG_MUJS
struct ecmascript_timeout *ecmascript_set_timeout2m(js_State *J, const char *handle, int timeout, int timeout_next);
#endif

int get_ecmascript_enable(struct ecmascript_interpreter *interpreter);

void check_for_rerender(struct ecmascript_interpreter *interpreter, const char* text);

void toggle_ecmascript(struct session *ses);

void location_goto(struct document_view *doc_view, char *url, int reload);
void location_goto_const(struct document_view *doc_view, const char *url, int reload);

extern char *console_error_filename;
extern char *console_log_filename;
extern char *console_warn_filename;

extern char *local_storage_filename;
extern int local_storage_ready;

#ifdef __cplusplus
}
#endif

#endif

#endif
