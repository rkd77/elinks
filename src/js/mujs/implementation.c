/* The MuJS domimplementation object. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/libdom/doc.h"
#include "js/ecmascript.h"
#include "js/libdom/parse.h"
#include "js/mujs.h"
#include "js/mujs/document.h"
#include "js/mujs/implementation.h"
#include "util/conv.h"

static void
mjs_implementation_createHTMLDocument(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *title = js_tostring(J, 1);

	if (!title) {
		js_error(J, "!title");
		return;
	}
	struct string str;

	if (!init_string(&str)) {
		js_error(J, "out of memory");
		return;
	}
	add_to_string(&str, "<!doctype html>\n<html><head><title>");
	add_html_to_string(&str, title, strlen(title));
	add_to_string(&str, "</title></head><body></body></html>");

	void *docu = document_parse_text("utf-8", str.source, str.length);
	done_string(&str);
	mjs_push_document(J, docu);
}

static void
mjs_implementation_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[implementation object]");
}

void
mjs_push_implementation(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	js_newobject(J);
	{
		addmethod(J, "Implementation.prototype.createHTMLDocument", mjs_implementation_createHTMLDocument, 1);
		addmethod(J, "Implementation.prototype.toString", mjs_implementation_toString, 0);
	}
}
