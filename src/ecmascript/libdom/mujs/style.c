/* The MuJS attr object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/style.h"

static void
mjs_style(js_State *J, const char *property)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception exc;
	dom_node *el = (struct dom_node *)js_touserdata(J, 0, "style");
	dom_string *style = NULL;
	const char *res = NULL;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_style, &style);

	if (exc != DOM_NO_ERR) {
		js_pushstring(J, "");
		return;
	}

	if (!style || !dom_string_length(style)) {
		js_pushstring(J, "");

		if (style) {
			dom_string_unref(style);
		}
		return;
	}

	res = get_css_value(dom_string_data(style), property);
	dom_string_unref(style);

	if (!res) {
		js_pushstring(J, "");
		return;
	}
	js_pushstring(J, res);
	mem_free(res);
}

static void
mjs_style_get_property_background(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "background");
}

static void
mjs_style_get_property_backgroundColor(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "background-color");
}

static void
mjs_style_get_property_color(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "color");
}
static void
mjs_style_get_property_display(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "display");
}

static void
mjs_style_get_property_fontStyle(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "font-style");
}

static void
mjs_style_get_property_fontWeight(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "font-weight");
}

static void
mjs_style_get_property_listStyle(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "list-style");
}

static void
mjs_style_get_property_listStyleType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "list-style-type");
}

static void
mjs_style_get_property_textAlign(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "text-align");
}

static void
mjs_style_get_property_textDecoration(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "text-decoration");
}

static void
mjs_style_get_property_whiteSpace(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_style(J, "white-space");
}


static void
mjs_style_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[style object]");
}

static
void mjs_style_finalizer(js_State *J, void *node)
{
}

void
mjs_push_style(js_State *J, void *node)
{
	js_newobject(J);
	{
		js_newuserdata(J, "style", node, mjs_style_finalizer);
		addmethod(J, "toString", mjs_style_toString, 0);
		addproperty(J, "background", mjs_style_get_property_background, NULL);
		addproperty(J, "backgroundColor", mjs_style_get_property_backgroundColor, NULL);
		addproperty(J, "color", mjs_style_get_property_color, NULL);
		addproperty(J, "display", mjs_style_get_property_display, NULL);
		addproperty(J, "fontStyle", mjs_style_get_property_fontStyle, NULL);
		addproperty(J, "fontWeight", mjs_style_get_property_fontWeight, NULL);
		addproperty(J, "listStyle", mjs_style_get_property_listStyle, NULL);
		addproperty(J, "listStyleType", mjs_style_get_property_listStyleType, NULL);
		addproperty(J, "textAlign", mjs_style_get_property_textAlign, NULL);
		addproperty(J, "textDecoration", mjs_style_get_property_textDecoration, NULL);
		addproperty(J, "whiteSpace", mjs_style_get_property_whiteSpace, NULL);
	}
}
