/* The MuJS Node implementation. */

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

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/mujs/attr.h"
#include "js/mujs/document.h"
#include "js/mujs/element.h"
#include "js/mujs/fragment.h"
#include "js/mujs/mapa.h"
#include "js/mujs/text.h"
#include "js/mujs.h"
#include "js/mujs/node.h"
#include "js/timer.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

enum {
	ELEMENT_NODE = 1,
	ATTRIBUTE_NODE = 2,
	TEXT_NODE = 3,
	CDATA_SECTION_NODE = 4,
	PROCESSING_INSTRUCTION_NODE = 7,
	COMMENT_NODE = 8,
	DOCUMENT_NODE = 9,
	DOCUMENT_TYPE_NODE = 10,
	DOCUMENT_FRAGMENT_NODE = 11
};

static void
mjs_node_static_get_property_ELEMENT_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, ELEMENT_NODE);
}

static void
mjs_node_static_get_property_ATTRIBUTE_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, ATTRIBUTE_NODE);
}

static void
mjs_node_static_get_property_TEXT_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, TEXT_NODE);
}

static void
mjs_node_static_get_property_CDATA_SECTION_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, CDATA_SECTION_NODE);
}

static void
mjs_node_static_get_property_PROCESSING_INSTRUCTION_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, PROCESSING_INSTRUCTION_NODE);
}

static void
mjs_node_static_get_property_COMMENT_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, COMMENT_NODE);
}

static void
mjs_node_static_get_property_DOCUMENT_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, DOCUMENT_NODE);
}

static void
mjs_node_static_get_property_DOCUMENT_TYPE_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, DOCUMENT_TYPE_NODE);
}
static void
mjs_node_static_get_property_DOCUMENT_FRAGMENT_NODE(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, DOCUMENT_FRAGMENT_NODE);
}

#if 0
static void
mjs_node_fun(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J);
}
#endif

void
mjs_push_node(js_State *J, void *n)
{
	ELOG
	dom_node *node = (dom_node *)n;
	dom_node_type typ;
	dom_exception exc = dom_node_get_node_type(node, &typ);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	switch (typ) {
	case ELEMENT_NODE:
		mjs_push_element(J, n);
		return;
	case ATTRIBUTE_NODE:
		mjs_push_attr(J, n);
		return;
	case TEXT_NODE:
	case CDATA_SECTION_NODE:
	case PROCESSING_INSTRUCTION_NODE:
	case COMMENT_NODE:
	default:
		mjs_push_text(J, n);
		return;
	case DOCUMENT_NODE:
		mjs_push_document2(J, n);
		return;
	case DOCUMENT_TYPE_NODE:
		mjs_push_doctype(J, n);
		return;
	case DOCUMENT_FRAGMENT_NODE:
		mjs_push_fragment(J, n);
		return;
	}
}



static void
mjs_node_constructor(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newobject(J);
	{
		addproperty(J, "ELEMENT_NODE", mjs_node_static_get_property_ELEMENT_NODE, NULL);
		addproperty(J, "ATTRIBUTE_NODE", mjs_node_static_get_property_ATTRIBUTE_NODE, NULL);
		addproperty(J, "TEXT_NODE", mjs_node_static_get_property_TEXT_NODE, NULL);
		addproperty(J, "CDATA_SECTION_NODE", mjs_node_static_get_property_CDATA_SECTION_NODE, NULL);
		addproperty(J, "PROCESSING_INSTRUCTION_NODE", mjs_node_static_get_property_PROCESSING_INSTRUCTION_NODE, NULL);
		addproperty(J, "COMMENT_NODE", mjs_node_static_get_property_COMMENT_NODE, NULL);
		addproperty(J, "DOCUMENT_NODE", mjs_node_static_get_property_DOCUMENT_NODE, NULL);
		addproperty(J, "DOCUMENT_TYPE_NODE", mjs_node_static_get_property_DOCUMENT_TYPE_NODE, NULL);
		addproperty(J, "DOCUMENT_FRAGMENT_NODE", mjs_node_static_get_property_DOCUMENT_FRAGMENT_NODE, NULL);
	}
}

int
mjs_node_init(js_State *J)
{
	ELOG
	js_pushglobal(J);
	//js_newcconstructor(J, mjs_node_fun, mjs_node_constructor, "Node", 0);
	mjs_node_constructor(J);
	js_defglobal(J, "Node", JS_DONTENUM);
	return 0;
}
