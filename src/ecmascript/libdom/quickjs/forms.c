/* The QuickJS object forms. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/form.h"
#include "ecmascript/quickjs/forms.h"
#include "ecmascript/quickjs/input.h"
#include "ecmascript/quickjs/window.h"
#include "viewer/text/form.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

void *map_forms;
void *map_rev_forms;

static void
forms_SetOpaque(JSValueConst this_val, void *node)
{
	REF_JS(this_val);

	if (!node) {
		attr_erase_from_map_rev(map_rev_forms, this_val);
	} else {
		attr_save_in_map_rev(map_rev_forms, this_val, node);
	}
}

/* Find the form whose name is @name, which should normally be a
 * string (but might not be).  */
static JSValue
js_find_form_by_name(JSContext *ctx,
		  struct document_view *doc_view,
		  const char *string)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form *form;

	if (!*string)
		return JS_NULL;

	foreach (form, doc_view->document->forms) {
		if (form->name && !c_strcasecmp(string, form->name)) {
			return js_get_form_object(ctx, JS_NULL, form);
		}
	}

	return JS_NULL;
}

static void
js_forms_set_items(JSContext *ctx, JSValueConst this_val, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	int counter = 0;
	struct form_view *fv;

	foreach (fv, vs->forms) {
		struct form *form = find_form_by_form_view(document, fv);
		JSValue v = js_get_form_object(ctx, JS_NULL, form);

		REF_JS(v);

		JS_SetPropertyUint32(ctx, this_val, counter, JS_DupValue(ctx, v));

		if (form->name) {
			if (strcmp(form->name, "item") && strcmp(form->name, "namedItem")) {
				JS_SetPropertyStr(ctx, this_val, form->name, JS_DupValue(ctx, v));
			}
		}
		JS_FreeValue(ctx, v);
		counter++;
	}
}

static JSValue
js_forms_get_property_length(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_UNDEFINED;
	}
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	return JS_NewInt32(ctx, list_size(&document->forms));
}

static JSValue
js_forms_item2(JSContext *ctx, JSValueConst this_val, int index)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct form_view *fv;
	int counter = -1;

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);

	vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			struct form *form = find_form_by_form_view(document, fv);

			return js_get_form_object(ctx, JS_NULL, form);
		}
	}

	return JS_UNDEFINED;
}

/* @forms_funcs{"item"} */
static JSValue
js_forms_item(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}
	int index;

	if (JS_ToInt32(ctx, &index, argv[0])) {
		return JS_UNDEFINED;
	}

	return js_forms_item2(ctx, this_val, index);
}

/* @forms_funcs{"namedItem"} */
static JSValue
js_forms_namedItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	if (argc != 1) {
		return JS_UNDEFINED;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, argv[0]);

	if (!str) {
		return JS_EXCEPTION;
	}
	JSValue ret = js_find_form_by_name(ctx, doc_view, str);
	JS_FreeCString(ctx, str);

	RETURN_JS(ret);
}

#if 0
JSString *
unicode_to_jsstring(JSContext *ctx, unicode_val_T u)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	char16_t buf[2];

	/* This is supposed to make a string from which
	 * jsval_to_accesskey() can get the original @u back.
	 * If @u is a surrogate, then that is not possible, so
	 * return NULL to indicate an error instead.
	 *
	 * If JS_NewUCStringCopyN hits a null character, it truncates
	 * the string there and pads it with more nulls.  However,
	 * that is not a problem here, because if there is a null
	 * character in buf[], then it must be the only character.  */
	if (u <= 0xFFFF && !is_utf16_surrogate(u)) {
		buf[0] = u;
		return JS_NewUCStringCopyN(ctx, buf, 1);
	} else if (needs_utf16_surrogates(u)) {
		buf[0] = get_utf16_high_surrogate(u);
		buf[1] = get_utf16_low_surrogate(u);
		return JS_NewUCStringCopyN(ctx, buf, 2);
	} else {
		return NULL;
	}
}
#endif

static JSValue
js_forms_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[forms object]");
}

static const JSCFunctionListEntry js_forms_proto_funcs[] = {
	JS_CGETSET_DEF("length", js_forms_get_property_length, NULL),
	JS_CFUNC_DEF("item", 1, js_forms_item),
	JS_CFUNC_DEF("namedItem", 1, js_forms_namedItem),
	JS_CFUNC_DEF("toString", 0, js_forms_toString)
};

JSValue
getForms(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue second;
	static int initialized;
	if (!initialized) {
		map_forms = attr_create_new_forms_map();
		map_rev_forms = attr_create_new_forms_map_rev();
		initialized = 1;
	}
	second = attr_find_in_map(map_forms, node);

	if (!JS_IsNull(second)) {
		JSValue r = JS_DupValue(ctx, second);
		RETURN_JS(r);
	}
	JSValue forms_obj = JS_NewArray(ctx);
	JS_SetPropertyFunctionList(ctx, forms_obj, js_forms_proto_funcs, countof(js_forms_proto_funcs));
	forms_SetOpaque(forms_obj, node);
	js_forms_set_items(ctx, forms_obj, node);
	attr_save_in_map(map_forms, node, forms_obj);

	JSValue rr = JS_DupValue(ctx, forms_obj);
	RETURN_JS(rr);
}
