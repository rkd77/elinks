/* The MuJS DOMParser implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/libdom/doc.h"
#include "js/ecmascript.h"
#include "js/libdom/dom.h"
#include "js/mujs.h"
#include "js/mujs/document.h"
#include "js/mujs/domparser.h"
#include "intl/charsets.h"
#include "terminal/event.h"

static void mjs_domparser_parseFromString(js_State *J);

static void
mjs_domparser_parseFromString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_pushnull(J);
		return;
	}
	dom_html_document *doc = (dom_html_document *)document_parse_text("utf-8", str, strlen(str));

	if (!doc) {
		js_pushnull(J);
		return;
	}
	mjs_push_document2(J, doc);
}

static void
mjs_domparser_fun(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}

static void
mjs_domparser_constructor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		addmethod(J, "DOMParser.prototype.parseFromString", mjs_domparser_parseFromString, 2);
	}
}

int
mjs_domparser_init(js_State *J)
{
	js_pushglobal(J);
	js_newcconstructor(J, mjs_domparser_fun, mjs_domparser_constructor, "DOMParser", 0);
	js_defglobal(J, "DOMParser", JS_DONTENUM);

	return 0;
}
