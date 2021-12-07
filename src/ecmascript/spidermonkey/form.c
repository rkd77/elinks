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

//static JSClass form_class;	     /* defined below */

static bool form_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool form_get_property_action(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_set_property_action(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_get_property_elements(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_get_property_encoding(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_set_property_encoding(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_get_property_method(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_set_property_method(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_set_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp);
static bool form_set_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp);

static void form_finalize(JSFreeOp *op, JSObject *obj);

static JSClassOps form_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	form_finalize,  // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

/* Each @form_class object must have a @document_class parent.  */
JSClass form_class = {
	"form",
	JSCLASS_HAS_PRIVATE,	/* struct form_view *, or NULL if detached */
	&form_ops
};

void
spidermonkey_detach_form_state(struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *jsinput = fs->ecmascript_obj;

	if (jsinput) {
//		JS::RootedObject r_jsinput(spidermonkey_empty_context, jsinput);
		/* This assumes JS_GetInstancePrivate and JS_SetPrivate
		 * cannot GC.  */

		/* If this assertion fails, it is not clear whether
		 * the private pointer of jsinput should be reset;
		 * crashes seem possible either way.  Resetting it is
		 * easiest.  */
//		assert(JS_GetInstancePrivate(spidermonkey_empty_context,
//					     r_jsinput,
//					     &input_class, NULL)
//		       == fs);
//		if_assert_failed {}

		JS_SetPrivate(jsinput, NULL);
		fs->ecmascript_obj = NULL;
	}
}

void
spidermonkey_moved_form_state(struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *jsinput = fs->ecmascript_obj;

	if (jsinput) {
		/* This assumes JS_SetPrivate cannot GC.  If it could,
		 * then the GC might call input_finalize for some
		 * other object whose struct form_state has also been
		 * reallocated, and an assertion would fail in
		 * input_finalize.  */
		JS_SetPrivate(jsinput, fs);
	}
}


static JSObject *
get_form_control_object(JSContext *ctx,
			enum form_type type, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	switch (type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
		case FC_CHECKBOX:
		case FC_RADIO:
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
		case FC_SELECT:
			return get_input_object(ctx, fs);

		case FC_TEXTAREA:
			/* TODO */
			return NULL;

		default:
			INTERNAL("Weird fc->type %d", type);
			return NULL;
	}
}


static struct form_view *form_get_form_view(JSContext *ctx, JS::HandleObject jsform, JS::Value *argv);
static bool form_elements_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void elements_finalize(JSFreeOp *op, JSObject *obj);

static JSClassOps form_elements_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	elements_finalize, // finalize
	nullptr,  // call
	nullptr,  // hasInstance
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

/* Each @form_elements_class object must have a @form_class parent.  */
static JSClass form_elements_class = {
	"elements",
	JSCLASS_HAS_PRIVATE,
	&form_elements_ops
};

static bool form_set_items(JSContext *ctx, JS::HandleObject hobj, void *node);
static bool form_set_items2(JSContext *ctx, JS::HandleObject hobj, void *node);

static bool form_elements_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp);
static bool form_elements_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *string, JS::MutableHandleValue hvp);
static bool form_elements_item(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool form_elements_namedItem(JSContext *ctx, unsigned int argc, JS::Value *rval);


static const spidermonkeyFunctionSpec form_elements_funcs[] = {
	{ "item",		form_elements_item,		1 },
	{ "namedItem",		form_elements_namedItem,	1 },
	{ NULL }
};

static bool form_elements_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (elements[INT] for INT>=0 is equivalent to
 * elements.item(INT)).  ECMAScript code should not use these directly
 * as in elements[-1]; future versions of ELinks may change the numbers.  */
enum form_elements_prop {
	JSP_FORM_ELEMENTS_LENGTH = -1,
};
static JSPropertySpec form_elements_props[] = {
	JS_PSG("length",	form_elements_get_property_length, JSPROP_ENUMERATE),
	JS_PS_END
};

static void
elements_finalize(JSFreeOp *op, JSObject *obj)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_view *fv = JS_GetPrivate(obj);

	if (fv) {
		/* If this assertion fails, leave fv->ecmascript_obj
		 * unchanged, because it may point to a different
		 * JSObject whose private pointer will later have to
		 * be updated to avoid crashes.  */
///		assert(fv->ecmascript_obj == obj);
///		if_assert_failed return;

		fv->ecmascript_obj = NULL;
		/* No need to JS_SetPrivate, because the object is
		 * being destroyed.  */
	}
}

static bool
form_set_items(JSContext *ctx, JS::HandleObject hobj, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;

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
	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

#if 0
	if (JSID_IS_STRING(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		char *string = jsval_to_string(ctx, r_idval);

		if (string) {
			xmlpp::ustring test = string;
			if (test == "item" || test == "namedItem") {
				mem_free(string);
				return true;
			}

			form_elements_namedItem2(ctx, hobj, string, hvp);
			mem_free(string);
		}
		return true;
	}
#endif

	int counter = 0;
	struct el_form_control *fc;
	foreach (fc, form->items) {
		struct form_state *fs = find_form_state(doc_view, fc);

		if (!fs) {
			continue;
		}

		JSObject *obj = get_form_control_object(ctx, fc->type, fs);
		if (!obj) {
			continue;
		}
		JS::RootedObject v(ctx, obj);
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));
		JS_SetElement(ctx, hobj, counter, ro);
		if (fc->id) {
			if (strcmp(fc->id, "item") && strcmp(fc->id, "namedItem")) {
				JS_DefineProperty(ctx, hobj, fc->id, ro, JSPROP_ENUMERATE | JSPROP_RESOLVING);
			}
		} else if (fc->name) {
			if (strcmp(fc->name, "item") && strcmp(fc->name, "namedItem")) {
				JS_DefineProperty(ctx, hobj, fc->name, ro, JSPROP_ENUMERATE | JSPROP_RESOLVING);
			}
		}
		counter++;
	}

	return true;
}

static bool
form_set_items2(JSContext *ctx, JS::HandleObject hobj, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;

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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form = node;

	int counter = 0;
	struct el_form_control *fc;
	foreach (fc, form->items) {
		struct form_state *fs = find_form_state(doc_view, fc);

		if (!fs) {
			continue;
		}

		JSObject *obj = get_form_control_object(ctx, fc->type, fs);
		if (!obj) {
			continue;
		}
		JS::RootedObject v(ctx, obj);
		JS::RootedValue ro(ctx, JS::ObjectOrNullValue(v));

		if (fc->id) {
			if (strcmp(fc->id, "item") && strcmp(fc->id, "namedItem")) {
				JS_DefineProperty(ctx, hobj, fc->id, ro, JSPROP_ENUMERATE);
			}
		} else if (fc->name) {
			if (strcmp(fc->name, "item") && strcmp(fc->name, "namedItem")) {
				JS_DefineProperty(ctx, hobj, fc->name, ro, JSPROP_ENUMERATE);
			}
		}
	}

	return true;
}

static bool
form_elements_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	jsid id = hid.get();

	JS::Value idval;
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;

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
	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	if (JSID_IS_STRING(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		char *string = jsval_to_string(ctx, r_idval);

		if (string) {
			xmlpp::ustring test = string;
			if (test == "item" || test == "namedItem") {
				mem_free(string);
				return true;
			}

			form_elements_namedItem2(ctx, hobj, string, hvp);
			mem_free(string);
		}
		return true;
	}

	if (!JSID_IS_INT(id)) {
		return true;
	}

	hvp.setUndefined();

	switch (JSID_TO_INT(hid)) {
	case JSP_FORM_ELEMENTS_LENGTH:
		hvp.setInt32(list_size(&form->items));
		break;
	default:
		/* Array index. */
		int index;
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		index = r_idval.toInt32();
		form_elements_item2(ctx, hobj, index, hvp);
		break;
	}

	return true;
}

static bool
form_elements_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}

	form = find_form_by_form_view(document, form_view);
	args.rval().setInt32(list_size(&form->items));

	return true;
}


/* @form_elements_funcs{"item"} */
static bool
form_elements_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	int index = args[0].toInt32();

	bool ret = form_elements_item2(ctx, hobj, index, &rval);
	args.rval().set(rval);

	return ret;
}

static bool
form_elements_item2(JSContext *ctx, JS::HandleObject hobj, int index, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct el_form_control *fc;
	int counter = -1;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);

	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	hvp.setUndefined();

	foreach (fc, form->items) {
		counter++;
		if (counter == index) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				JSObject *fcobj = get_form_control_object(ctx, fc->type, fs);

				if (fcobj) {
					hvp.setObject(*fcobj);
				}
			}
			break;
		}
	}

	return true;
}

/* @form_elements_funcs{"namedItem"} */
static bool
form_elements_namedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);

	char *string = jsval_to_string(ctx, args[0]);

	bool ret = form_elements_namedItem2(ctx, hobj, string, &rval);
	args.rval().set(rval);
	mem_free_if(string);

	return ret;
}

static bool
form_elements_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *string, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!*string) {
		return true;
	}

	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
	if (!form_view) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	form = find_form_by_form_view(document, form_view);

	hvp.setUndefined();

	foreach (fc, form->items) {
		if ((fc->id && !c_strcasecmp(string, fc->id))
		    || (fc->name && !c_strcasecmp(string, fc->name))) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				JSObject *fcobj = get_form_control_object(ctx, fc->type, fs);

				if (fcobj) {
					hvp.setObject(*fcobj);
				}
			}
			break;
		}
	}

	return true;
}



/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in form[-1];
 * future versions of ELinks may change the numbers.  */
enum form_prop {
	JSP_FORM_ACTION   = -1,
	JSP_FORM_ELEMENTS = -2,
	JSP_FORM_ENCODING = -3,
	JSP_FORM_LENGTH   = -4,
	JSP_FORM_METHOD   = -5,
	JSP_FORM_NAME     = -6,
	JSP_FORM_TARGET   = -7,
};

static JSPropertySpec form_props[] = {
	JS_PSGS("action",	form_get_property_action, form_set_property_action, JSPROP_ENUMERATE),
	JS_PSG("elements",	form_get_property_elements, JSPROP_ENUMERATE),
	JS_PSGS("encoding",	form_get_property_encoding, form_set_property_encoding, JSPROP_ENUMERATE),
	JS_PSG("length",	form_get_property_length, JSPROP_ENUMERATE),
	JS_PSGS("method",	form_get_property_method, form_set_property_method, JSPROP_ENUMERATE),
	JS_PSGS("name",	form_get_property_name, form_set_property_name, JSPROP_ENUMERATE),
	JS_PSGS("target",	form_get_property_target, form_set_property_target, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool form_reset(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool form_submit(JSContext *ctx, unsigned int argc, JS::Value *rval);

static const spidermonkeyFunctionSpec form_funcs[] = {
	{ "reset",	form_reset,	0 },
	{ "submit",	form_submit,	0 },
	{ NULL }
};

static struct form_view *
form_get_form_view(JSContext *ctx, JS::HandleObject r_jsform, JS::Value *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_view *fv = JS_GetInstancePrivate(ctx, r_jsform,
						     &form_elements_class,
						     NULL);

	if (!fv) return NULL;	/* detached */

	assert(fv->ecmascript_obj == r_jsform);
	if_assert_failed return NULL;
	
	return fv;
}

/* @form_class.getProperty */
static bool
form_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	ELINKS_CAST_PROP_PARAMS
	jsid id = hid.get();
	/* DBG("doc %p %s\n", parent_doc, JS_GetStringBytes(JS_ValueToString(ctx, OBJECT_TO_JSVAL(parent_doc)))); */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;

	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);

	assert(form);

	if (JSID_IS_STRING(hid)) {
		struct el_form_control *fc;
		char *string = jsid_to_string(ctx, hid);

		foreach (fc, form->items) {
			JSObject *fcobj = NULL;
			struct form_state *fs;

			if ((!fc->id || c_strcasecmp(string, fc->id))
			    && (!fc->name || c_strcasecmp(string, fc->name)))
				continue;

			hvp.setUndefined();
			fs = find_form_state(doc_view, fc);
			if (fs) {
				fcobj = get_form_control_object(ctx, fc->type, fs);
				if (fcobj) {
					hvp.setObject(*fcobj);
				}
			}
			break;
		}
		return true;
	}

	if (!JSID_IS_INT(hid))
		return true;

	hvp.setUndefined();

	return true;
}


static bool
form_get_property_action(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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

	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);
	args.rval().setString(JS_NewStringCopyZ(ctx, form->action));

	return true;
}

static bool
form_set_property_action(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	char *string;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	string = jsval_to_string(ctx, args[0]);
	if (form->action) {
		ecmascript_set_action(&form->action, string);
	} else {
		mem_free_set(&form->action, string);
	}

	return true;
}

static bool
form_get_property_elements(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);
	struct view_state *vs = interpreter->vs;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct form *form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);

	if (!form) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct form_view *fv = nullptr;
	bool found = false;

	foreach (fv, vs->forms) {
		if (fv->form_num == form->form_num) {
			found = true;
			break;
		}
	}

	if (!found || !fv) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	/* jsform ('form') is form_elements' parent; who knows is that's correct */
	JSObject *jsform_elems = JS_NewObjectWithGivenProto(ctx, &form_elements_class, hobj);
	JS::RootedObject r_jsform_elems(ctx, jsform_elems);

	JS_DefineProperties(ctx, r_jsform_elems, (JSPropertySpec *) form_elements_props);
	spidermonkey_DefineFunctions(ctx, jsform_elems,
				     form_elements_funcs);
	JS_SetPrivate(jsform_elems, fv);
	fv->ecmascript_obj = jsform_elems;

	form_set_items(ctx, r_jsform_elems, fv);

	args.rval().setObject(*r_jsform_elems);

	return true;
}

static bool
form_get_property_encoding(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	switch (form->method) {
	case FORM_METHOD_GET:
	case FORM_METHOD_POST:
		args.rval().setString(JS_NewStringCopyZ(ctx, "application/x-www-form-urlencoded"));
		break;
	case FORM_METHOD_POST_MP:
		args.rval().setString(JS_NewStringCopyZ(ctx, "multipart/form-data"));
		break;
	case FORM_METHOD_POST_TEXT_PLAIN:
		args.rval().setString(JS_NewStringCopyZ(ctx, "text/plain"));
		break;
	}

	return true;
}

/* @form_class.setProperty */
static bool
form_set_property_encoding(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	char *string;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	string = jsval_to_string(ctx, args[0]);
	if (!string) {
		return true;
	}
	if (!c_strcasecmp(string, "application/x-www-form-urlencoded")) {
		form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
		                                               : FORM_METHOD_POST;
	} else if (!c_strcasecmp(string, "multipart/form-data")) {
		form->method = FORM_METHOD_POST_MP;
	} else if (!c_strcasecmp(string, "text/plain")) {
		form->method = FORM_METHOD_POST_TEXT_PLAIN;
	}
	mem_free(string);

	return true;
}

static bool
form_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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

	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	args.rval().setInt32(list_size(&form->items));

	return true;
}

static bool
form_get_property_method(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	switch (form->method) {
	case FORM_METHOD_GET:
		args.rval().setString(JS_NewStringCopyZ(ctx, "GET"));
		break;

	case FORM_METHOD_POST:
	case FORM_METHOD_POST_MP:
	case FORM_METHOD_POST_TEXT_PLAIN:
		args.rval().setString(JS_NewStringCopyZ(ctx, "POST"));
		break;
	}

	return true;
}

/* @form_class.setProperty */
static bool
form_set_property_method(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	char *string;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	string = jsval_to_string(ctx, args[0]);
	if (!string) {
		return true;
	}
	if (!c_strcasecmp(string, "GET")) {
		form->method = FORM_METHOD_GET;
	} else if (!c_strcasecmp(string, "POST")) {
		form->method = FORM_METHOD_POST;
	}
	mem_free(string);

	return true;
}

static bool
form_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	args.rval().setString(JS_NewStringCopyZ(ctx, form->name));

	return true;
}

/* @form_class.setProperty */
static bool
form_set_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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

	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);
	mem_free_set(&form->name, jsval_to_string(ctx, args[0]));

	return true;
}

static bool
form_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);
	args.rval().setString(JS_NewStringCopyZ(ctx, form->target));

	return true;
}

static bool
form_set_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
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
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL)) {
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
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);
	mem_free_set(&form->target, jsval_to_string(ctx, args[0]));

	return true;
}


/* @form_funcs{"reset"} */
static bool
form_reset(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &form_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	args.rval().setBoolean(false);

	return true;
}

/* @form_funcs{"submit"} */
static bool
form_submit(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::Value val;
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	struct form_view *fv;
	struct form *form;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &form_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;

	form = JS_GetInstancePrivate(ctx, hobj, &form_class, nullptr);
	assert(form);
	submit_given_form(ses, doc_view, form, 0);

	args.rval().setBoolean(false);

	return true;
}

JSObject *
get_form_object(JSContext *ctx, JSObject *jsdoc, struct form *form)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JSObject *jsform = form->ecmascript_obj;

	if (jsform) {
		JS::RootedObject r_jsform(ctx, jsform);
		/* This assumes JS_GetInstancePrivate cannot GC.  */
		assert(JS_GetInstancePrivate(ctx, r_jsform,
					     &form_class, NULL)
		       == form);
		if_assert_failed return NULL;

		return jsform;
	}

	/* jsdoc ('document') is fv's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */
	jsform = JS_NewObject(ctx, &form_class);
	if (jsform == NULL)
		return NULL;
	JS::RootedObject r_jsform(ctx, jsform);
	JS_DefineProperties(ctx, r_jsform, form_props);
	spidermonkey_DefineFunctions(ctx, jsform, form_funcs);
	JS_SetPrivate(jsform, form); /* to @form_class */
	form->ecmascript_obj = jsform;
	form_set_items2(ctx, r_jsform, form);

	return jsform;
}

static void
form_finalize(JSFreeOp *op, JSObject *jsform)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form *form = JS_GetPrivate(jsform);

	if (form) {
		/* If this assertion fails, leave fv->ecmascript_obj
		 * unchanged, because it may point to a different
		 * JSObject whose private pointer will later have to
		 * be updated to avoid crashes.  */
		assert(form->ecmascript_obj == jsform);
		if_assert_failed return;

		form->ecmascript_obj = NULL;
		/* No need to JS_SetPrivate, because the object is
		 * being destroyed.  */
	}
}

void
spidermonkey_detach_form_view(struct form_view *fv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSObject *jsform = fv->ecmascript_obj;

	if (jsform) {
//		JS::RootedObject r_jsform(spidermonkey_empty_context, jsform);
		/* This assumes JS_GetInstancePrivate and JS_SetPrivate
		 * cannot GC.  */

		/* If this assertion fails, it is not clear whether
		 * the private pointer of jsform should be reset;
		 * crashes seem possible either way.  Resetting it is
		 * easiest.  */
//		assert(JS_GetInstancePrivate(spidermonkey_empty_context,
//					     r_jsform,
//					     &form_class, NULL)
//		       == fv);
//		if_assert_failed {}

		JS_SetPrivate(jsform, NULL);
		fv->ecmascript_obj = NULL;
	}
}
