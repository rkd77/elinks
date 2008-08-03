#ifndef EL__ECMASCRIPT_ECMASCRIPT_H
#define EL__ECMASCRIPT_ECMASCRIPT_H

/* This is a trivial ECMAScript driver. All your base are belong to pasky. */
/* In the future you will get DOM, a complete ECMAScript interface and free
 * plasm displays for everyone. */

#include "main/module.h"
#include "util/time.h"

struct form_state;
struct form_view;
struct string;
struct terminal;
struct uri;
struct view_state;

#define get_ecmascript_enable()		get_opt_bool("ecmascript.enable")

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

	time_t exec_start;

	/* This is a cross-rerenderings accumulator of
	 * @document.onload_snippets (see its description for juicy details).
	 * They enter this list as they continue to appear there, and they
	 * never leave it (so that we can always find from where to look for
	 * any new snippets in document.onload_snippets). Instead, as we
	 * go through the list we maintain a pointer to the last processed
	 * entry. */
	LIST_OF(struct string_list_item) onload_snippets;
	struct string_list_item *current_onload_snippet;

	/* ID of the {struct document} where those onload_snippets belong to.
	 * It is kept at 0 until it is definitively hard-attached to a given
	 * final document. Then if we suddenly appear with this structure upon
	 * a document with a different ID, we reset the state and start with a
	 * fresh one (normally, that does not happen since reloading sets
	 * ecmascript_fragile, but it can happen i.e. when the urrent document
	 * is reloaded in another tab and then you just cause the current tab
	 * to redraw. */
	unsigned int onload_snippets_cache_id;
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

int ecmascript_check_url(unsigned char *url, unsigned char *frame);
void ecmascript_free_urls(struct module *module);

struct ecmascript_interpreter *ecmascript_get_interpreter(struct view_state*vs);
void ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter);

void ecmascript_detach_form_view(struct form_view *fv);
void ecmascript_detach_form_state(struct form_state *fs);
void ecmascript_moved_form_state(struct form_state *fs);

void ecmascript_reset_state(struct view_state *vs);

void ecmascript_eval(struct ecmascript_interpreter *interpreter, struct string *code, struct string *ret);
unsigned char *ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter, struct string *code);
/* Returns -1 if undefined. */
int ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

/* Takes line with the syntax javascript:<ecmascript code>. Activated when user
 * follows a link with this synstax. */
void ecmascript_protocol_handler(struct session *ses, struct uri *uri);

void ecmascript_timeout_dialog(struct terminal *term, int max_exec_time);

void ecmascript_set_action(unsigned char **action, unsigned char *string);

void ecmascript_set_timeout(struct ecmascript_interpreter *interpreter, unsigned char *code, int timeout);

extern struct module ecmascript_module;

#endif
