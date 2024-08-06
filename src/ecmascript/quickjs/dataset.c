/* The QuickJS dataset object implementation. */

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
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/dataset.h"
#include "ecmascript/quickjs/element.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

JSClassID js_dataset_class_id;

static void
js_dataset_finalizer(JSRuntime *rt, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);
	dom_node *el = (dom_node *)(JS_GetOpaque(val, js_dataset_class_id));

	if (el) {
		dom_node_unref(el);
	}
}

/* return < 0 if exception, or TRUE/FALSE */
static int
js_obj_delete_property(JSContext *ctx, JSValueConst obj, JSAtom prop)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	const char *property = JS_AtomToCString(ctx, prop);

	if (!property) {
		return 0;
	}
	dom_node *el = (dom_node *)(JS_GetOpaque(obj, js_dataset_class_id));
	struct string data;

	if (!el ||!init_string(&data)) {
		JS_FreeCString(ctx, property);
		return 0;
	}
	camel_to_html(property, &data);
	JS_FreeCString(ctx, property);

	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return 0;
	}
	(void)dom_element_remove_attribute(el, attr_name);
	dom_string_unref(attr_name);
	interpreter->changed = true;

	return 1;
}

#if 0
/* The following methods can be emulated with the previous ones,
   so they are usually not needed */
/* return < 0 if exception or TRUE/FALSE */
static int
js_obj_has_property(JSContext *ctx, JSValueConst obj, JSAtom atom)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	fprintf(stderr, "has_property\n");
	return 1;
}
#endif

static JSValue
js_obj_get_property(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst receiver)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *property = JS_AtomToCString(ctx, atom);

	if (!property) {
		return JS_UNDEFINED;
	}
	dom_node *el = (dom_node *)(JS_GetOpaque(obj, js_dataset_class_id));
	struct string data;

	if (!el ||!init_string(&data)) {
		JS_FreeCString(ctx, property);
		return JS_UNDEFINED;
	}
	camel_to_html(property, &data);
	JS_FreeCString(ctx, property);

	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return JS_UNDEFINED;
	}
	dom_string *attr_value = NULL;
	exc = dom_element_get_attribute(el, attr_name, &attr_value);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr_value) {
		return JS_UNDEFINED;
	}
	JSValue ret = JS_NewStringLen(ctx, dom_string_data(attr_value), dom_string_length(attr_value));
	dom_string_unref(attr_value);

	RETURN_JS(ret);
}

/* return < 0 if exception or TRUE/FALSE */
static int
js_obj_set_property(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst val, JSValueConst receiver, int flags)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	const char *property = JS_AtomToCString(ctx, atom);

	if (!property) {
		return 0;
	}
	size_t len;
	const char *value = JS_ToCStringLen(ctx, &len, val);

	if (!value) {
		JS_FreeCString(ctx, property);
		return 0;
	}
	dom_node *el = (dom_node *)(JS_GetOpaque(obj, js_dataset_class_id));
	struct string data;

	if (!el ||!init_string(&data)) {
		JS_FreeCString(ctx, property);
		JS_FreeCString(ctx, value);
		return 0;
	}
	camel_to_html(property, &data);
	JS_FreeCString(ctx, property);

	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		JS_FreeCString(ctx, value);
		return 0;
	}
	dom_string *attr_value = NULL;
	exc = dom_string_create((const uint8_t *)value, strlen(value), &attr_value);
	JS_FreeCString(ctx, value);

	if (exc != DOM_NO_ERR || !attr_value) {
		dom_string_unref(attr_name);
		return 0;
	}
	exc = dom_element_set_attribute(el, attr_name, attr_value);
	dom_string_unref(attr_name);
	dom_string_unref(attr_value);
	interpreter->changed = true;

	return 1;
}

static JSClassExoticMethods exo = {
	.delete_property = js_obj_delete_property,
//	.has_property = js_obj_has_property,
	.get_property = js_obj_get_property,
	.set_property = js_obj_set_property
};

static JSClassDef js_dataset_class = {
	"dataset",
	.finalizer = js_dataset_finalizer,
	.exotic = &exo
};

JSValue
getDataset(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* nodelist class */
	static int initialized;

	if (!initialized) {
		JS_NewClassID(&js_dataset_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_dataset_class_id, &js_dataset_class);
		initialized = 1;
	}
	JSValue proto = JS_NewObjectClass(ctx, js_dataset_class_id);
	REF_JS(proto);

//	JS_SetPropertyFunctionList(ctx, proto, js_dataset_proto_funcs, countof(js_dataset_proto_funcs));
	JS_SetClassProto(ctx, js_dataset_class_id, proto);
	dom_node_ref(node);
	JS_SetOpaque(proto, node);
	JSValue rr = JS_DupValue(ctx, proto);

	RETURN_JS(rr);
}
