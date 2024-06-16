/* The SpiderMonkey attributes object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/libdom/dom.h"

#include "ecmascript/spidermonkey/util.h"
#include <jsfriendapi.h>

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
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey/attr.h"
#include "ecmascript/spidermonkey/attributes.h"
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

static bool attributes_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool attributes_getNamedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool attributes_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool attributes_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp);

static void attributes_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

#if 0
	dom_namednodemap *attrs = (dom_namednodemap *)JS::GetMaybePtrFromReservedSlot<dom_namednodemap>(obj, 0);

	if (attrs) {
		dom_namednodemap_unref(attrs);
	}
#endif
}

JSClassOps attributes_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	attributes_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

JSClass attributes_class = {
	"attributes",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&attributes_ops
};

static const spidermonkeyFunctionSpec attributes_funcs[] = {
	{ "item",		attributes_item,		1 },
	{ "getNamedItem",		attributes_getNamedItem,	1 },
	{ NULL }
};

static bool attributes_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

static JSPropertySpec attributes_props[] = {
	JS_PSG("length",	attributes_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool
attributes_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
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

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	dom_exception err;
	dom_namednodemap *attrs = (dom_namednodemap *)(node);
	unsigned long idx;
	uint32_t num_attrs;

	if (!attrs) {
		return true;
	}

	err = dom_namednodemap_get_length(attrs, &num_attrs);
	if (err != DOM_NO_ERR) {
		//dom_namednodemap_unref(attrs);
		return true;
	}

	for (idx = 0; idx < num_attrs; ++idx) {
		dom_attr *attr;
		dom_string *name = NULL;

		err = dom_namednodemap_item(attrs, idx, (void *)&attr);

		if (err != DOM_NO_ERR || !attr) {
			continue;
		}
		JSObject *obj = getAttr(ctx, attr);

		if (!obj) {
			continue;
		}
		JS::RootedObject v(ctx, obj);
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
		JS_SetElement(ctx, hobj, idx, ro);
		err = dom_attr_get_name(attr, &name);

		if (err != DOM_NO_ERR) {
			goto next;
		}

		if (name && !dom_string_caseless_lwc_isequal(name, corestring_lwc_item) && !dom_string_caseless_lwc_isequal(name, corestring_lwc_nameditem)) {
			JS_DefineProperty(ctx, hobj, dom_string_data(name), ro, JSPROP_ENUMERATE);
		}
next:
		if (name) {
			dom_string_unref(name);
		}
		dom_node_unref(attr);
	}

	return true;
}

static bool
attributes_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
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
	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	dom_exception err;
	dom_namednodemap *attrs;
	uint32_t num_attrs;

	attrs = JS::GetMaybePtrFromReservedSlot<dom_namednodemap>(hobj, 0);

	if (!attrs) {
		args.rval().setInt32(0);
		return true;
	}
	err = dom_namednodemap_get_length(attrs, &num_attrs);
	if (err != DOM_NO_ERR) {
		//dom_namednodemap_unref(attrs);
		args.rval().setInt32(0);
		return true;
	}
	args.rval().setInt32(num_attrs);

	return true;
}

static bool
attributes_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();
	bool ret = attributes_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
attributes_getNamedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *str = jsval_to_string(ctx, args[0]);
	bool ret = attributes_namedItem2(ctx, hobj, str, &rval);
	args.rval().set(rval);

	mem_free_if(str);

	return ret;
}

static bool
attributes_item2(JSContext *ctx, JS::HandleObject hobj, int idx, JS::MutableHandleValue hvp)
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

	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	hvp.setUndefined();

	dom_exception err;
	dom_namednodemap *attrs;
	dom_attr *attr;

	attrs = (dom_namednodemap *)JS::GetMaybePtrFromReservedSlot<dom_namednodemap>(hobj, 0);

	if (!attrs) {
		return true;
	}

	err = dom_namednodemap_item(attrs, idx, (void *)&attr);

	if (err != DOM_NO_ERR) {
		return true;
	}
	JSObject *obj = (JSObject *)getAttr(ctx, attr);
	hvp.setObject(*obj);
	dom_node_unref(attr);

	return true;
}

static bool
attributes_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *str, JS::MutableHandleValue hvp)
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

	if (!JS_InstanceOf(ctx, hobj, &attributes_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	hvp.setUndefined();

	dom_exception err;
	dom_namednodemap *attrs;
	dom_attr *attr = NULL;
	dom_string *name = NULL;

	attrs = (dom_namednodemap *)JS::GetMaybePtrFromReservedSlot<dom_namednodemap>(hobj, 0);

	if (!attrs) {
		return true;
	}
	err = dom_string_create((const uint8_t*)str, strlen(str), &name);

	if (err != DOM_NO_ERR) {
		return true;
	}
	err = dom_namednodemap_get_named_item(attrs, name, &attr);
	dom_string_unref(name);

	if (err != DOM_NO_ERR || !attr) {
		return true;
	}
	JSObject *obj = (JSObject *)getAttr(ctx, attr);
	hvp.setObject(*obj);
	dom_node_unref(attr);

	return true;
}

JSObject *
getAttributes(JSContext *ctx, void *node)
{
	JSObject *el = JS_NewObject(ctx, &attributes_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) attributes_props);
	spidermonkey_DefineFunctions(ctx, el, attributes_funcs);

	JS::SetReservedSlot(el, 0, JS::PrivateValue(node));
	attributes_set_items(ctx, r_el, node);

	return el;
}
