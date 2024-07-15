/* The MuJS nodeList2 object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/nodelist2.h"
#include "ecmascript/mujs/window.h"

void *map_nodelist2;
void *map_rev_nodelist2;

static void
mjs_push_nodeList2_item2(js_State *J, int idx)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)(js_touserdata(J, 0, "nodelist2"));

	if (!sni) {
		js_pushundefined(J);
		return;
	}
	int counter = 0;
	struct selector_node *sn = NULL;

	foreach (sn, *sni) {
		if (counter == idx) {
			break;
		}
		counter++;
	}

	if (!sn || !sn->node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, sn->node);
}

static void
mjs_nodeList2_item(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	int index = js_toint32(J, 1);

	mjs_push_nodeList2_item2(J, index);
}

static void
mjs_nodeList2_set_items(js_State *J, void *nodes)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)nodes;
	uint32_t i = 0;

	if (!sni) {
		return;
	}
	struct selector_node *sn = NULL;

	foreach (sn, *sni) {
		mjs_push_element(J, sn->node);
		js_setindex(J, -2, i);
		i++;
	}
	js_setlength(J, -1, i);
}

static void
mjs_nodeList2_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[nodeList2 object]");
}

static void
mjs_nodeList2_finalizer(js_State *J, void *node)
{
	attr_erase_from_map(map_nodelist2, node);
}

#if 0
static void
mjs_nodeList2_get_property_length(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	LIST_OF(struct selector_node) *sni = (LIST_OF(struct selector_node) *)(js_touserdata(J, 0, "nodelist2"));
	uint32_t length;

	if (!sni) {
		js_pushnumber(J, 0);
		return;
	}
	length = list_size(sni);
	js_pushnumber(J, length);
}
#endif

void
mjs_push_nodelist2(js_State *J, void *nodes)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_newarray(J);
	{
		js_newuserdata(J, "nodelist2", nodes, mjs_nodeList2_finalizer);
//		addproperty(J, "length", mjs_nodeList_get_property_length, NULL);
		addmethod(J, "item", mjs_nodeList2_item, 1);
		addmethod(J, "toString", mjs_nodeList2_toString, 0);
		mjs_nodeList2_set_items(J, nodes);
	}
	attr_save_in_map(map_nodelist2, nodes, nodes);
}
