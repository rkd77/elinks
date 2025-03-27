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

#include "js/ecmascript.h"
#include "js/mujs/mapa.h"
#include "js/mujs.h"
#include "js/mujs/attr.h"

static void
mjs_attr_get_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	dom_exception exc;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	dom_attr *attr = (dom_attr *)(js_touserdata(J, 0, "attr"));

	if (!attr) {
		js_pushnull(J);
		return;
	}
	dom_string *name = NULL;
	exc = dom_attr_get_name(attr, &name);

	if (exc != DOM_NO_ERR || name == NULL) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(name));
	dom_string_unref(name);
}

static void
mjs_attr_get_property_value(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	dom_exception exc;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	dom_attr *attr = (dom_attr *)(js_touserdata(J, 0, "attr"));

	if (!attr) {
		js_pushnull(J);
		return;
	}
	dom_string *value = NULL;
	exc = dom_attr_get_value(attr, &value);

	if (exc != DOM_NO_ERR || value == NULL) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(value));
	dom_string_unref(value);
}

static void
mjs_attr_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[attr object]");
}

void *map_attrs;

static
void mjs_attr_finalizer(js_State *J, void *node)
{
	attr_erase_from_map(map_attrs, node);

#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref((dom_node *)node);
}

void
mjs_push_attr(js_State *J, void *node)
{
	js_newobject(J);
	{
		js_newuserdata(J, "attr", node, mjs_attr_finalizer);
		addmethod(J, "toString", mjs_attr_toString, 0);
		addproperty(J, "name", mjs_attr_get_property_name, NULL);
		addproperty(J, "value", mjs_attr_get_property_value, NULL);
	}
}
