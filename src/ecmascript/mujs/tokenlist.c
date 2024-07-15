/* The MuJS DOM tokenlist object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/tokenlist.h"

static void
mjs_tokenlist_add(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_tokenlist *tl = (dom_tokenlist *)(js_touserdata(J, 0, "tokenlist"));

	if (!tl) {
		js_pushundefined(J);
		return;
	}
	const char *klass = js_tostring(J, 1);

	if (!klass) {
		js_pushundefined(J);
		return;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, strlen(klass), &kl);

	if (exc != DOM_NO_ERR || !kl) {
		js_pushundefined(J);
		return;
	}
	exc = dom_tokenlist_add(tl, kl);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		interpreter->changed = true;
	}
	js_pushundefined(J);
}

static void
mjs_tokenlist_contains(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_tokenlist *tl = (dom_tokenlist *)(js_touserdata(J, 0, "tokenlist"));

	if (!tl) {
		js_pushundefined(J);
		return;
	}
	const char *klass = js_tostring(J, 1);

	if (!klass) {
		js_pushundefined(J);
		return;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, strlen(klass), &kl);

	if (exc != DOM_NO_ERR || !kl) {
		js_pushundefined(J);
		return;
	}
	bool res = false;
	exc = dom_tokenlist_contains(tl, kl, &res);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		js_pushboolean(J, res);
		return;
	}
	js_pushundefined(J);
}

static void
mjs_tokenlist_remove(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_tokenlist *tl = (dom_tokenlist *)(js_touserdata(J, 0, "tokenlist"));

	if (!tl) {
		js_pushundefined(J);
		return;
	}
	const char *klass = js_tostring(J, 1);

	if (!klass) {
		js_pushundefined(J);
		return;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, strlen(klass), &kl);

	if (exc != DOM_NO_ERR || !kl) {
		js_pushundefined(J);
		return;
	}
	exc = dom_tokenlist_remove(tl, kl);
	dom_string_unref(kl);

	if (exc == DOM_NO_ERR) {
		interpreter->changed = true;
	}
	js_pushundefined(J);
}

static void
mjs_tokenlist_toggle(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_tokenlist *tl = (dom_tokenlist *)(js_touserdata(J, 0, "tokenlist"));

	if (!tl) {
		js_pushundefined(J);
		return;
	}
	const char *klass = js_tostring(J, 1);

	if (!klass) {
		js_pushundefined(J);
		return;
	}
	dom_string *kl = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)klass, strlen(klass), &kl);

	if (exc != DOM_NO_ERR || !kl) {
		js_pushundefined(J);
		return;
	}
	bool res = false;
	exc = dom_tokenlist_contains(tl, kl, &res);

	if (exc == DOM_NO_ERR) {
		if (res) {
			exc = dom_tokenlist_remove(tl, kl);
		} else {
			exc = dom_tokenlist_add(tl, kl);
		}
		if (exc == DOM_NO_ERR) {
			interpreter->changed = true;
		}
	}
	dom_string_unref(kl);
	js_pushundefined(J);
}

static void
mjs_tokenlist_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[tokenlist object]");
}

static void
mjs_tokenlist_finalizer(js_State *J, void *node)
{
	dom_tokenlist *tl = (dom_tokenlist *)(node);

	if (tl) {
		dom_tokenlist_unref(tl);
	}
}

void
mjs_push_tokenlist(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		js_newuserdata(J, "tokenlist", node, mjs_tokenlist_finalizer);
		addmethod(J, "add", mjs_tokenlist_add, 1);
		addmethod(J, "contains", mjs_tokenlist_contains, 1);
		addmethod(J, "remove", mjs_tokenlist_remove, 1);
		addmethod(J, "toggle", mjs_tokenlist_toggle, 1);
		addmethod(J, "toString", mjs_tokenlist_toString, 0);
	}
}
