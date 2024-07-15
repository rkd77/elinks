/* The MuJS CSSStyleDeclaration object implementation. */

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
#include "ecmascript/mujs/css.h"
#include "ecmascript/mujs/element.h"

static void
mjs_CSSStyleDeclaration_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, 3); // fake
}

#if 0
static void
mjs_push_CSSStyleDeclaration_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "0"); // fake
}
#endif

static void
mjs_CSSStyleDeclaration_getPropertyValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushundefined(J); // fake
}

static void
mjs_CSSStyleDeclaration_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "0"); // fake
}

#if 0
static void
mjs_push_CSSStyleDeclaration_namedItem2(js_State *J, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "0"); // fake
}
#endif

static void
mjs_CSSStyleDeclaration_namedItem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "0");
}

static void
mjs_CSSStyleDeclaration_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "0");
	js_setproperty(J, -2, "marginTop");
	js_pushstring(J, "0");
	js_setproperty(J, -2, "marginLeft");
	js_pushstring(J, "0");
	js_setproperty(J, -2, "marginRight");
}

static void
mjs_CSSStyleDeclaration_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[CSSStyleDeclaration object]");
}

static void
mjs_CSSStyleDeclaration_finalizer(js_State *J, void *node)
{
}

void
mjs_push_CSSStyleDeclaration(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newarray(J);
	{
		js_newuserdata(J, "CSSStyleDeclaration", node, mjs_CSSStyleDeclaration_finalizer);
		addmethod(J, "getPropertyValue", mjs_CSSStyleDeclaration_getPropertyValue, 1);
		addmethod(J, "item", mjs_CSSStyleDeclaration_item, 1);
		addmethod(J, "namedItem", mjs_CSSStyleDeclaration_namedItem, 1);
		addmethod(J, "toString", mjs_CSSStyleDeclaration_toString, 0);
		addproperty(J, "length", mjs_CSSStyleDeclaration_get_property_length, NULL);
		mjs_CSSStyleDeclaration_set_items(J, node);
	}
}
