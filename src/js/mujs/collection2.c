/* The MuJS html collection2 object implementation. */

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
#include "js/ecmascript-c.h"
#include "js/ecmascript.h"
#include "js/mujs/mapa.h"
#include "js/mujs.h"
#include "js/mujs/collection.h"
#include "js/mujs/element.h"
#include "js/mujs/node.h"

static void
mjs_htmlCollection2_get_property_length(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_touserdata(J, 0, "collection2"));
	uint32_t size;

	if (!ns) {
		js_pushnumber(J, 0);
		return;
	}
	if (dom_html_collection_get_length((dom_html_collection *)ns, &size) != DOM_NO_ERR) {
		js_pushnumber(J, 0);
		return;
	}
	js_pushnumber(J, size);
}

static void
mjs_push_htmlCollection2_item2(js_State *J, int idx)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_touserdata(J, 0, "collection2"));
	dom_node *node;
	dom_exception err;

	if (!ns) {
		js_pushundefined(J);
		return;
	}
	err = dom_html_collection_item((dom_html_collection *)ns, idx, &node);

	if (err != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}
	mjs_push_node(J, node);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_htmlCollection2_item(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_htmlCollection2_item2(J, index);
}

static void
mjs_push_htmlCollection2_namedItem2(js_State *J, const char *str)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)(js_touserdata(J, 0, "collection2"));
	dom_exception err;
	dom_string *name;
	uint32_t size, i;

	if (!ns) {
		js_pushundefined(J);
		return;
	}

	if (dom_html_collection_get_length((dom_html_collection *)ns, &size) != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}

	err = dom_string_create((const uint8_t*)str, strlen(str), &name);

	if (err != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *val = NULL;

		err = dom_html_collection_item((dom_html_collection *)ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}

		err = dom_element_get_attribute(element, corestring_dom_id, &val);

		if (err == DOM_NO_ERR && val) {
			if (dom_string_caseless_isequal(name, val)) {
				mjs_push_node(J, element);
				dom_string_unref(val);
				dom_string_unref(name);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(element);

				return;
			}
			dom_string_unref(val);
		}

		err = dom_element_get_attribute(element, corestring_dom_name, &val);

		if (err == DOM_NO_ERR && val) {
			if (dom_string_caseless_isequal(name, val)) {
				mjs_push_node(J, element);
				dom_string_unref(val);
				dom_string_unref(name);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(element);

				return;
			}
			dom_string_unref(val);
		}
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(element);
	}
	dom_string_unref(name);
	js_pushundefined(J);
}

static void
mjs_htmlCollection2_namedItem(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *str = js_tostring(J, 1);

	mjs_push_htmlCollection2_namedItem2(J, str);
}

static void
mjs_htmlCollection2_set_items(js_State *J, void *node)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int counter = 0;
	uint32_t size, i;
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)node;
	dom_exception err;

	if (!ns) {
		return;
	}
	dom_html_collection_ref((dom_html_collection *)ns);

	if (dom_html_collection_get_length((dom_html_collection *)ns, &size) != DOM_NO_ERR) {
		dom_html_collection_unref((dom_html_collection *)ns);
		return;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *name = NULL;
		err = dom_html_collection_item((dom_html_collection *)ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		mjs_push_node(J, element);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(element);
		js_setindex(J, -2, counter);
		err = dom_element_get_attribute(element, corestring_dom_id, &name);

		if (err != DOM_NO_ERR || !name) {
			err = dom_element_get_attribute(element, corestring_dom_name, &name);
		}

		if (err == DOM_NO_ERR && name) {
			if (!dom_string_caseless_lwc_isequal(name, corestring_lwc_item) && !dom_string_caseless_lwc_isequal(name, corestring_lwc_nameditem)) {
				if (js_try(J)) {
					js_pop(J, 1);
					goto next;
				}
				mjs_push_node(J, element);
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(element);
				js_setproperty(J, -2, dom_string_data(name));
				js_endtry(J);
			}
		}
next:
		counter++;
		dom_string_unref(name);
	}
	dom_html_collection_unref((dom_html_collection *)ns);
}

static void
mjs_htmlCollection2_toString(js_State *J)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[htmlCollection2 object]");
}

static void
mjs_htmlCollection2_finalizer(js_State *J, void *node)
{
	ELOG
	struct el_dom_html_collection *ns = (struct el_dom_html_collection *)node;

	if (ns) {
		//attr_erase_from_map_str(map_collections, ns);
		if (ns->refcnt > 0) {
			if (ns->ctx) {
				free_el_dom_collection(ns->ctx);
				ns->ctx = NULL;
			}
			dom_html_collection_unref((dom_html_collection *)ns);
		}
	}
}

void
mjs_push_collection2(js_State *J, void *node)
{
	ELOG
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newarray(J);
	{
		js_newuserdata(J, "collection2", node, mjs_htmlCollection2_finalizer);
		addmethod(J, "htmlCollection2.prototype.item", mjs_htmlCollection2_item, 1);
		addmethod(J, "htmlCollection2.prototype.namedItem", mjs_htmlCollection2_namedItem, 1);
		addmethod(J, "htmlCollection2.prototype.toString", mjs_htmlCollection2_toString, 0);
		addproperty(J, "length", mjs_htmlCollection2_get_property_length, NULL);
		mjs_htmlCollection2_set_items(J, node);
	}
	//attr_save_in_map(map_collections, node, node);
}
