/* The SpiderMonkey window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

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
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "ecmascript/spidermonkey/document.h"
#include "ecmascript/spidermonkey/form.h"
#include "ecmascript/spidermonkey/forms.h"
#include "ecmascript/spidermonkey/input.h"
#include "ecmascript/spidermonkey/window.h"
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

#include <libxml++/libxml++.h>

static bool forms_set_items(JSContext *ctx, JS::HandleObject hobj, void *node);
static bool forms_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool forms_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

JSClassOps forms_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	nullptr,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

/* Each @forms_class object must have a @document_class parent.  */
JSClass forms_class = {
	"forms",
	JSCLASS_HAS_PRIVATE,
	&forms_ops
};

static bool forms_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool forms_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool forms_namedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);

const spidermonkeyFunctionSpec forms_funcs[] = {
	{ "item",		forms_item,		1 },
	{ "namedItem",		forms_namedItem,	1 },
	{ NULL }
};

/* Tinyids of properties.  Use negative values to distinguish these from
 * array indexes (forms[INT] for INT>=0 is equivalent to forms.item(INT)).
 * ECMAScript code should not use these directly as in forms[-1];
 * future versions of ELinks may change the numbers.  */
enum forms_prop {
	JSP_FORMS_LENGTH = -1,
};
JSPropertySpec forms_props[] = {
	JS_PSG("length",	forms_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

/* Find the form whose name is @name, which should normally be a
 * string (but might not be).  If found, set *rval = the DOM
 * object.  If not found, leave *rval unchanged.  */
static void
find_form_by_name(JSContext *ctx,
		  struct document_view *doc_view,
		  char *string, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form *form;

	if (!*string)
		return;

	foreach (form, doc_view->document->forms) {
		if (form->name && !c_strcasecmp(string, form->name)) {
			hvp.setObject(*get_form_object(ctx, nullptr, form));
			break;
		}
	}
}

static bool
forms_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	struct view_state *vs;
	struct document_view *doc_view;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	int counter = 0;
	struct form_view *fv;

	foreach (fv, vs->forms) {
		struct form *form = find_form_by_form_view(document, fv);
		JS::RootedObject v(ctx, get_form_object(ctx, nullptr, form));
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
		JS_SetElement(ctx, hobj, counter, ro);

		if (form->name) {
			if (strcmp(form->name, "item") && strcmp(form->name, "namedItem")) {
				JS_DefineProperty(ctx, hobj, form->name, ro, JSPROP_ENUMERATE | JSPROP_RESOLVING);
			}
		}
		counter++;
	}

#if 0
	if (JSID_IS_STRING(hid)) {
		char *string = jsid_to_string(ctx, hid);
		xmlpp::ustring test = string;

		if (test == "item" || test == "namedItem") {
			return true;
		}
		find_form_by_name(ctx, doc_view, string, hvp);

		return true;
	}
#endif

	return true;
}


/* @forms_class.getProperty */
static bool
forms_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
//	jsid id = hid.get();

	JS::Value idval;
//	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (JSID_IS_STRING(hid)) {
		char *string = jsid_to_string(ctx, hid);
		xmlpp::ustring test = string;

		if (test == "item" || test == "namedItem") {
			return true;
		}
		find_form_by_name(ctx, doc_view, string, hvp);

		return true;
	}
	/* Array index. */
	JS::RootedValue r_idval(ctx, idval);
	JS_IdToValue(ctx, hid, &r_idval);
	int index = r_idval.toInt32();
	forms_item2(ctx, hobj, index, hvp);

	return true;
}

static bool
forms_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL)) {
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
	doc_view = vs->doc_view;
	document = doc_view->document;
	args.rval().setInt32(list_size(&document->forms));

	return true;
}

/* @forms_funcs{"item"} */
static bool
forms_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);


//	JS::Value *argv = JS_ARGV(ctx, rval);
	int index = args[0].toInt32();
	bool ret = forms_item2(ctx, hobj, index, &rval);

	args.rval().set(rval.get());

	return ret;
}

static bool
forms_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
//	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct form_view *fv;
	int counter = -1;

	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;

	hvp.setUndefined();

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			struct form *form = find_form_by_form_view(document, fv);
			hvp.setObject(*get_form_object(ctx, nullptr, form));
			break;
		}
	}

	return true;
}

/* @forms_funcs{"namedItem"} */
static bool
forms_namedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
//	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &forms_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (argc != 1)
		return true;

	char *string = jsval_to_string(ctx, args[0]);

	JS::RootedValue rval(ctx, val);
	rval.setNull();

	find_form_by_name(ctx, doc_view, string, &rval);
	args.rval().set(rval.get());

	mem_free_if(string);

	return true;
}

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

/* Convert the string *@vp to an access key.  Return 0 for no access
 * key, UCS_NO_CHAR on error, or the access key otherwise.  */
static unicode_val_T
jsval_to_accesskey(JSContext *ctx, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	size_t len;
	char16_t chr[2];

	JSString *str = hvp.toString();

	len = JS_GetStringLength(str);

	/* This implementation ignores extra characters in the string.  */
	if (len < 1)
		return 0;	/* which means no access key */
	JS_GetStringCharAt(ctx, str, 0, &chr[0]);
	if (!is_utf16_surrogate(chr[0])) {
		return chr[0];
	}
	if (len >= 2) {
		JS_GetStringCharAt(ctx, str, 1, &chr[1]);
		if (is_utf16_high_surrogate(chr[0])
			&& is_utf16_low_surrogate(chr[1])) {
			return join_utf16_surrogates(chr[0], chr[1]);
		}
	}
	JS_ReportErrorUTF8(ctx, "Invalid UTF-16 sequence");
	return UCS_NO_CHAR;	/* which the caller will reject */
}

JSObject *
getForms(JSContext *ctx, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *el = JS_NewObject(ctx, &forms_class);

	if (!el) {
		return NULL;
	}

	JS::RootedObject r_el(ctx, el);

	JS_DefineProperties(ctx, r_el, (JSPropertySpec *) forms_props);
	spidermonkey_DefineFunctions(ctx, el, forms_funcs);

	JS_SetPrivate(el, node);
	forms_set_items(ctx, r_el, node);

	return r_el;
}
