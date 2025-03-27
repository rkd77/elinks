/* The MuJS nodeList object implementation. */

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

#include "js/ecmascript.h"
#include "js/mujs/mapa.h"
#include "js/mujs.h"
#include "js/mujs/element.h"
#include "js/mujs/node.h"
#include "js/mujs/nodelist.h"
#include "js/mujs/window.h"

//void *map_nodelist;
//void *map_rev_nodelist;

static void
mjs_push_nodeList_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	dom_nodelist *nl = (dom_nodelist *)(js_touserdata(J, 0, "nodelist"));
	dom_node *element = NULL;
	dom_exception err;

	if (!nl) {
		js_pushundefined(J);
		return;
	}
	err = dom_nodelist_item(nl, idx, (void *)&element);

	if (err != DOM_NO_ERR || !element) {
		js_pushundefined(J);
		return;
	}
	mjs_push_node(J, element);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(element);
}

static void
mjs_nodeList_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_nodeList_item2(J, index);
}

static void
mjs_nodeList_set_items(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_nodelist *nl = (dom_nodelist *)(node);
	dom_exception err;
	uint32_t length, i;

	if (!nl) {
		return;
	}
	err = dom_nodelist_get_length(nl, &length);

	if (err != DOM_NO_ERR) {
		return;
	}

	for (i = 0; i < length; i++) {
		dom_node *element = NULL;
		err = dom_nodelist_item(nl, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		mjs_push_node(J, element);
		js_setindex(J, -2, i);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(element);
	}
	js_setlength(J, -1, length);
}

static void
mjs_nodeList_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[nodeList object]");
}

static void
mjs_nodeList_finalizer(js_State *J, void *node)
{
	//attr_erase_from_map(map_nodelist, node);
	dom_nodelist_unref((dom_nodelist *)node);
}

#if 0
static void
mjs_nodeList_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_nodelist *nl = (dom_nodelist *)(js_touserdata(J, 0, "nodelist"));
	dom_exception err;
	uint32_t length;

	if (!nl) {
		js_pushnumber(J, 0);
		return;
	}
	err = dom_nodelist_get_length(nl, &length);

	if (err != DOM_NO_ERR) {
		js_pushnumber(J, 0);
		return;
	}

	js_pushnumber(J, length);
}
#endif

void
mjs_push_nodelist(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_nodelist_ref((dom_nodelist *)node);
	js_newarray(J);
	{
		js_newuserdata(J, "nodelist", node, mjs_nodeList_finalizer);
//		addproperty(J, "length", mjs_nodeList_get_property_length, NULL);
		addmethod(J, "item", mjs_nodeList_item, 1);
		addmethod(J, "toString", mjs_nodeList_toString, 0);
		mjs_nodeList_set_items(J, node);
	}
	//attr_save_in_map(map_nodelist, node, node);
}
