/* The SpiderMonkey html collection object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "js/libdom/dom.h"

#include "js/spidermonkey/util.h"
#include <jsfriendapi.h>
#include <js/Array.h>

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/libdom/corestrings.h"
#include "document/view.h"
#include "js/ecmascript.h"
#include "js/spidermonkey/collection.h"
#include "js/spidermonkey/element.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
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

#include <iostream>
#include <algorithm>
#include <string>

static bool htmlCollection2_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool htmlCollection2_namedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool htmlCollection2_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool htmlCollection2_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp);

static void htmlCollection_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_html_collection *ns = JS::GetMaybePtrFromReservedSlot<dom_html_collection>(obj, 0);

	if (ns) {
		dom_html_collection_unref(ns);
	}
}

static void htmlCollection2_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_html_collection *ns = JS::GetMaybePtrFromReservedSlot<dom_html_collection>(obj, 0);

	if (ns) {
		dom_html_collection_unref(ns);
	}
}

static bool
col_obj_getProperty(JSContext* ctx, JS::HandleObject obj, JS::HandleValue receiver, JS::HandleId id, JS::MutableHandleValue vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (id.isString()) {
		char *property = jsid_to_string(ctx, id);

		if (property) {
			if (!strcmp(property, "item") || !strcmp(property, "namedItem")) {
				mem_free(property);
				JSObject *col = &(JS::GetReservedSlot(obj, 1).toObject());

				if (!col) {
					vp.setUndefined();
					return true;
				}
				JS::RootedObject r_col(ctx, col);

				return JS_GetPropertyById(ctx, r_col, id, vp);
			}
			mem_free(property);
		}
	}
	JSObject *arr = &(JS::GetReservedSlot(obj, 2).toObject());

	if (!arr) {
		vp.setUndefined();
		return true;
	}
	JS::RootedObject r_arr(ctx, arr);

	return JS_GetPropertyById(ctx, r_arr, id, vp);
}

static bool
col_obj_setProperty(JSContext* ctx, JS::HandleObject obj, JS::HandleId id, JS::HandleValue v, JS::HandleValue receiver, JS::ObjectOpResult& result)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *arr = &(JS::GetReservedSlot(obj, 2).toObject());

	if (!arr) {
		return true;
	}
	JS::RootedObject r_arr(ctx, arr);
	(void)JS_SetPropertyById(ctx, r_arr, id, v);

	return result.succeed();
}

static bool
col_obj_deleteProperty(JSContext* ctx, JS::HandleObject obj, JS::HandleId id, JS::ObjectOpResult& result)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *arr = &(JS::GetReservedSlot(obj, 2).toObject());

	if (!arr) {
		return true;
	}
	JS::RootedObject r_arr(ctx, arr);
	(void)JS_DeletePropertyById(ctx, r_arr, id);

	return result.succeed();
}

JSClassOps htmlCollection_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	htmlCollection_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClassOps htmlCollection2_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	htmlCollection2_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

js::ObjectOps col_obj_ops = {
	.getProperty = col_obj_getProperty,
	.setProperty = col_obj_setProperty,
	.deleteProperty = col_obj_deleteProperty
};

JSClass htmlCollection_class = {
	"htmlCollection",
	JSCLASS_HAS_RESERVED_SLOTS(3),
	&htmlCollection_ops,
	.oOps = &col_obj_ops
};

JSClass htmlCollection2_class = {
	"htmlCollection2",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&htmlCollection2_ops
};

static const spidermonkeyFunctionSpec htmlCollection2_funcs[] = {
	{ "item",		htmlCollection2_item,		1 },
	{ "namedItem",		htmlCollection2_namedItem,	1 },
	{ NULL }
};

static bool htmlCollection2_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec htmlCollection2_props[] = {
	JS_PSG("length",	htmlCollection2_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
htmlCollection2_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
//	if (!JS_InstanceOf(ctx, hobj, &htmlCollection2_class, NULL)) {
//#ifdef ECMASCRIPT_DEBUG
//	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
//#endif
//		return false;
//	}

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_html_collection *ns = JS::GetMaybePtrFromReservedSlot<dom_html_collection>(hobj, 0);
	uint32_t size;

	if (!ns) {
		args.rval().setInt32(0);
		return true;
	}

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		args.rval().setInt32(0);
		return true;
	}
	args.rval().setInt32(size);

	return true;
}

static bool
htmlCollection2_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();
	bool ret = htmlCollection2_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
htmlCollection2_namedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *str = jsval_to_string(ctx, args[0]);
	rval.setNull();
	bool ret = htmlCollection2_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	mem_free_if(str);

	return ret;
}

static bool
htmlCollection2_item2(JSContext *ctx, JS::HandleObject hobj, int idx, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
//	if (!JS_InstanceOf(ctx, hobj, &htmlCollection_class, NULL)) {
//#ifdef ECMASCRIPT_DEBUG
//	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
//#endif
//		return false;
//	}
	hvp.setUndefined();
	dom_html_collection *ns = JS::GetMaybePtrFromReservedSlot<dom_html_collection>(hobj, 0);
	dom_node *node;
	dom_exception err;

	if (!ns) {
		return true;
	}
	err = dom_html_collection_item(ns, idx, &node);

	if (err != DOM_NO_ERR) {
		return true;
	}
	JSObject *obj = getElement(ctx, node);
	hvp.setObject(*obj);
	dom_node_unref(node);

	return true;
}

static bool
htmlCollection2_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

//	if (!JS_InstanceOf(ctx, hobj, &htmlCollection2_class, NULL)) {
//#ifdef ECMASCRIPT_DEBUG
//	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
//#endif
//		return false;
//	}
	hvp.setUndefined();

	dom_html_collection *ns = JS::GetMaybePtrFromReservedSlot<dom_html_collection>(hobj, 0);
	dom_exception err;
	dom_string *name;
	uint32_t size, i;

	if (!ns) {
		return true;
	}

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		return true;
	}

	err = dom_string_create((const uint8_t*)str, strlen(str), &name);

	if (err != DOM_NO_ERR) {
		return true;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *val = NULL;

		err = dom_html_collection_item(ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}

		err = dom_element_get_attribute(element, corestring_dom_id, &val);

		if (err == DOM_NO_ERR && val) {
			if (dom_string_caseless_isequal(name, val)) {
				JSObject *obj = (JSObject *)getElement(ctx, element);
				hvp.setObject(*obj);

				dom_string_unref(val);
				dom_string_unref(name);
				dom_node_unref(element);

				return true;
			}
			dom_string_unref(val);
		}

		err = dom_element_get_attribute(element, corestring_dom_name, &val);

		if (err == DOM_NO_ERR && val) {
			if (dom_string_caseless_isequal(name, val)) {
				JSObject *obj = (JSObject *)getElement(ctx, element);
				hvp.setObject(*obj);

				dom_string_unref(val);
				dom_string_unref(name);
				dom_node_unref(element);

				return true;
			}
			dom_string_unref(val);
		}
		dom_node_unref(element);
	}
	dom_string_unref(name);

	return true;
}

static bool
htmlCollection_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	int counter = 0;
	uint32_t size, i;
	dom_html_collection *ns = (dom_html_collection *)node;
	dom_exception err;

	if (!ns) {
		return true;
	}

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		return true;
	}

	for (i = 0; i < size; i++) {
		dom_node *element = NULL;
		dom_string *name = NULL;
		err = dom_html_collection_item(ns, i, &element);

		if (err != DOM_NO_ERR || !element) {
			continue;
		}
		JSObject *obj = getElement(ctx, element);

		if (!obj) {
			continue;
		}
		JS::RootedObject v(ctx, obj);
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
		JS_SetElement(ctx, hobj, counter, ro);

		err = dom_element_get_attribute(element, corestring_dom_id, &name);

		if (err != DOM_NO_ERR || !name) {
			err = dom_element_get_attribute(element, corestring_dom_name, &name);
		}

		if (err == DOM_NO_ERR && name) {
			if (!dom_string_caseless_lwc_isequal(name, corestring_lwc_item) && !dom_string_caseless_lwc_isequal(name, corestring_lwc_nameditem)) {
				JS_DefineProperty(ctx, hobj, dom_string_data(name), ro, JSPROP_ENUMERATE);
			}
		}
		counter++;
		if (name) {
			dom_string_unref(name);
		}
		dom_node_unref(element);
	}

	return true;
}

JSObject *
getCollection(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_html_collection *ns = (dom_html_collection *)node;
	uint32_t size;

	if (!ns) {
		return NULL;
	}

	if (dom_html_collection_get_length(ns, &size) != DOM_NO_ERR) {
		return NULL;
	}
	JSObject *arr = JS::NewArrayObject(ctx, size);
	JS::RootedObject r_arr(ctx, arr);
	htmlCollection_set_items(ctx, r_arr, node);
	JSObject *col = JS_NewObject(ctx, &htmlCollection2_class);

	if (!col) {
		return NULL;
	}
	JS::RootedObject r_col(ctx, col);
	JS_DefineProperties(ctx, r_col, (JSPropertySpec *) htmlCollection2_props);
	spidermonkey_DefineFunctions(ctx, col, htmlCollection2_funcs);
	JS::SetReservedSlot(col, 0, JS::PrivateValue(node));

	JSObject *el = JS_NewObject(ctx, &htmlCollection_class);

	if (!el) {
		return NULL;
	}
	JS::RootedObject r_el(ctx, el);
	dom_html_collection_ref(ns);
	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));
	JS::SetReservedSlot(el, 1, JS::ObjectValue(*col));
	JS::SetReservedSlot(el, 2, JS::ObjectValue(*arr));

	return el;
}
