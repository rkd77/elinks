/* The MuJS attributes object implementation. */

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
#include "ecmascript/mujs/attr.h"
#include "ecmascript/mujs/attributes.h"

void *map_attributes;
void *map_rev_attributes;

static void
mjs_attributes_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception err;
	dom_namednodemap *attrs = (dom_namednodemap *)(node);
	unsigned long idx;
	uint32_t num_attrs;

	if (!attrs) {
		return;
	}
	err = dom_namednodemap_get_length(attrs, &num_attrs);

	if (err != DOM_NO_ERR) {
		//dom_namednodemap_unref(attrs);
		return;
	}
	for (idx = 0; idx < num_attrs; ++idx) {
		dom_attr *attr;
		dom_string *name = NULL;

		err = dom_namednodemap_item(attrs, idx, (void *)&attr);

		if (err != DOM_NO_ERR || !attr) {
			continue;
		}
// TODO Check it
		mjs_push_attr(J, attr);
		js_setindex(J, -2, idx);
		err = dom_attr_get_name(attr, &name);

		if (err != DOM_NO_ERR) {
			goto next;
		}

		if (name && !dom_string_caseless_lwc_isequal(name, corestring_lwc_item) && !dom_string_caseless_lwc_isequal(name, corestring_lwc_nameditem)) {
			if (js_try(J)) {
				js_pop(J, 1);
				goto next;
			}
			mjs_push_attr(J, attr);
			js_setproperty(J, -2, dom_string_data(name));
			js_endtry(J);
		}
next:
		if (name) {
			dom_string_unref(name);
		}
		dom_node_unref(attr);
	}
}

static void
mjs_attributes_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	dom_exception err;
	dom_namednodemap *attrs;
	uint32_t num_attrs;
	attrs = (dom_namednodemap *)(js_touserdata(J, 0, "attribute"));

	if (!attrs) {
		js_pushnumber(J, 0);
		return;
	}
	err = dom_namednodemap_get_length(attrs, &num_attrs);
	if (err != DOM_NO_ERR) {
		js_pushnumber(J, 0);
		return;
	}
	js_pushnumber(J, num_attrs);
}

static void
mjs_push_attributes_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception err;
	dom_namednodemap *attrs;
	dom_attr *attr = NULL;
	attrs = (dom_namednodemap *)(js_touserdata(J, 0, "attribute"));

	if (!attrs) {
		js_pushundefined(J);
		return;
	}

	err = dom_namednodemap_item(attrs, idx, (void *)&attr);

	if (err != DOM_NO_ERR || !attr) {
		js_pushundefined(J);
		return;
	}
	mjs_push_attr(J, attr);
	dom_node_unref(attr);
	js_pushundefined(J);
}

static void
mjs_attributes_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_attributes_item2(J, index);
}

static void
mjs_push_attributes_namedItem2(js_State *J, const char *str)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception err;
	dom_namednodemap *attrs;
	dom_attr *attr = NULL;
	dom_string *name = NULL;

	attrs = (dom_namednodemap *)(js_touserdata(J, 0, "attribute"));

	if (!attrs) {
		js_pushundefined(J);
		return;
	}
	err = dom_string_create((const uint8_t*)str, strlen(str), &name);

	if (err != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}
	err = dom_namednodemap_get_named_item(attrs, name, &attr);
	dom_string_unref(name);

	if (err != DOM_NO_ERR || !attr) {
		js_pushundefined(J);
		return;
	}
	mjs_push_attr(J, attr);
	dom_node_unref(attr);
	js_pushundefined(J);
}

static void
mjs_attributes_getNamedItem(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_push_attributes_namedItem2(J, str);
}

static void
mjs_attributes_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[attributes object]");
}

static void
mjs_attributes_finalizer(js_State *J, void *node)
{
	attr_erase_from_map(map_attributes, node);
}

void
mjs_push_attributes(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newarray(J);
	{
		js_newuserdata(J, "attribute", node, mjs_attributes_finalizer);
		addmethod(J, "item", mjs_attributes_item, 1);
		addmethod(J, "getNamedItem", mjs_attributes_getNamedItem, 1);
		addmethod(J, "toString", mjs_attributes_toString, 0);
		addproperty(J, "length", mjs_attributes_get_property_length, NULL);

		mjs_attributes_set_items(J, node);
	}
	attr_save_in_map(map_attributes, node, node);
}
