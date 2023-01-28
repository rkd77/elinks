/* The MuJS domimplementation object. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/implementation.h"
#include "util/conv.h"

#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>

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
	add_to_string(&str, title);
	add_to_string(&str, "</title></head><body></body></html>");

	// Parse HTML and create a DOM tree
	xmlDoc* doc = htmlReadDoc((xmlChar*)str.source, NULL, "utf-8",
	HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	// Encapsulate raw libxml document in a libxml++ wrapper
	xmlpp::Document *docu = new(std::nothrow) xmlpp::Document(doc);
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
		addmethod(J, "createHTMLDocument", mjs_implementation_createHTMLDocument, 1);
		addmethod(J, "toString", mjs_implementation_toString, 0);
	}
}
