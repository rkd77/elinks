#ifndef EL__JS_MUJS_DOCUMENT_H
#define EL__JS_MUJS_DOCUMENT_H

#include <mujs.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

struct el_listener {
	LIST_HEAD_EL(struct el_listener);
	char *typ;
	const char *fun;
};

enum readyState {
	LOADING = 0,
	INTERACTIVE,
	COMPLETE
};

struct mjs_document_private {
	struct ecmascript_interpreter *interpreter;
	const char *thisval;
	LIST_OF(struct el_listener) listeners;
	dom_event_listener *listener;
	void *node;
	const char *onkeydown;
	const char *onkeyup;
	int ref_count;
	enum readyState state;
};

void *mjs_doc_getprivate(js_State *J, int idx);
void mjs_push_document(js_State *J, void *doc);
void mjs_push_document2(js_State *J, void *doc);
void mjs_push_doctype(js_State *J, void *doc);
int mjs_document_init(js_State *J);

#ifdef __cplusplus
}
#endif

#endif
