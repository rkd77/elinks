/* The SpiderMonkey datasetobject implementation. */

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
#include <js/Id.h>

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
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/spidermonkey/dataset.h"
#include "ecmascript/spidermonkey/element.h"
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

static void dataset_finalize(JS::GCContext *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *element = JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);

	if (element) {
		dom_node_unref(element);
	}
}

static bool
dataset_obj_getProperty(JSContext* ctx, JS::HandleObject obj, JS::HandleValue receiver, JS::HandleId id, JS::MutableHandleValue vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	vp.setUndefined();

	if (!id.isString()) {
		return true;
	}
	char *property = jsid_to_string(ctx, id);

	if (!property) {
		return true;
	}
	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);
	struct string data;

	if (!el ||!init_string(&data)) {
		mem_free(property);
		return true;
	}
	camel_to_html(&data, property);
	mem_free(property);

	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create(data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return true;
	}
	dom_string *attr_value = NULL;
	exc = dom_element_get_attribute(el, attr_name, &attr_value);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr_value) {
		return true;
	}
	vp.setString(JS_NewStringCopyZ(ctx, dom_string_data(attr_value)));
	dom_string_unref(attr_value);

	return true;
}

static bool
dataset_obj_setProperty(JSContext* ctx, JS::HandleObject obj, JS::HandleId id, JS::HandleValue v, JS::HandleValue receiver, JS::ObjectOpResult& result)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!id.isString()) {
		return result.failInvalidDescriptor();
	}
	char *property = jsid_to_string(ctx, id);

	if (!property) {
		return result.failInvalidDescriptor();
	}
	char *value = jsval_to_string(ctx, v);

	if (!value) {
		mem_free(property);
		return result.failInvalidDescriptor();
	}
	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);
	struct string data;

	if (!el ||!init_string(&data)) {
		mem_free(property);
		mem_free(value);
		return result.failInvalidDescriptor();
	}
	camel_to_html(&data, property);
	mem_free(property);

	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create(data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		mem_free(value);
		return result.failInvalidDescriptor();
	}
	dom_string *attr_value = NULL;
	exc = dom_string_create(value, strlen(value), &attr_value);
	mem_free(value);

	if (exc != DOM_NO_ERR || !attr_value) {
		dom_string_unref(attr_name);
		return result.failInvalidDescriptor();
	}
	exc = dom_element_set_attribute(el, attr_name, attr_value);
	dom_string_unref(attr_name);
	dom_string_unref(attr_value);
	interpreter->changed = true;

	return result.succeed();
}

static bool
dataset_obj_deleteProperty(JSContext* ctx, JS::HandleObject obj, JS::HandleId id, JS::ObjectOpResult& result)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);
	if (!id.isString()) {
		return result.failCantDelete();
	}
	char *property = jsid_to_string(ctx, id);

	if (!property) {
		return result.failCantDelete();
	}
	dom_node *el = JS::GetMaybePtrFromReservedSlot<dom_node>(obj, 0);
	struct string data;

	if (!el ||!init_string(&data)) {
		mem_free(property);
		return result.failCantDelete();
	}
	camel_to_html(&data, property);
	mem_free(property);

	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create(data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return result.failCantDelete();
	}
	dom_string *attr_value = NULL;
	exc = dom_element_remove_attribute(el, attr_name);
	dom_string_unref(attr_name);
	interpreter->changed = true;

	return result.succeed();
}

JSClassOps dataset_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	dataset_finalize, // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

js::ObjectOps dataset_obj_ops = {
//	.hasProperty = dataset_obj_hasProperty,
	.getProperty = dataset_obj_getProperty,
	.setProperty = dataset_obj_setProperty,
	.deleteProperty = dataset_obj_deleteProperty
};

JSClass dataset_class = {
	"dataset",
	JSCLASS_HAS_RESERVED_SLOTS(1),
	&dataset_ops,
	.oOps = &dataset_obj_ops
};

static const spidermonkeyFunctionSpec dataset_funcs[] = {
	{ NULL }
};

static JSPropertySpec dataset_props[] = {
	JS_PS_END
};

JSObject *
getDataset(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *ds = JS_NewObject(ctx, &dataset_class);

	if (!ds) {
		return NULL;
	}
	JS::RootedObject r_el(ctx, ds);
//	JS_DefineProperties(ctx, r_el, (JSPropertySpec *)dataset_props);
//	spidermonkey_DefineFunctions(ctx, el, dataset_funcs);
	dom_node_ref(node);
	JS::SetReservedSlot(ds, 0, JS::PrivateValue(node));

	return ds;
}
