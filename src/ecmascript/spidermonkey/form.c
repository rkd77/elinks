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
#include "ecmascript/spidermonkey/window.h"
#include "intl/gettext/libintl.h"
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

#include <string>

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
	JS_PropertyStub, nullptr,
	form_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, form_finalize
};

/* Each @form_class object must have a @document_class parent.  */
static JSClass form_class = {
	"form",
	JSCLASS_HAS_PRIVATE,	/* struct form_view *, or NULL if detached */
	&form_ops
};



/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static bool input_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool input_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static void input_finalize(JSFreeOp *op, JSObject *obj);

static JSClassOps input_ops = {
	JS_PropertyStub, nullptr,
	input_get_property, input_set_property,
	nullptr, nullptr, nullptr, input_finalize
};

/* Each @input_class object must have a @form_class parent.  */
static JSClass input_class = {
	"input", /* here, we unleash ourselves */
	JSCLASS_HAS_PRIVATE,	/* struct form_state *, or NULL if detached */
	&input_ops
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (even though this object has no array elements).
 * ECMAScript code should not use these directly as in input[-1];
 * future versions of ELinks may change the numbers.  */
enum input_prop {
	JSP_INPUT_ACCESSKEY       = -1,
	JSP_INPUT_ALT             = -2,
	JSP_INPUT_CHECKED         = -3,
	JSP_INPUT_DEFAULT_CHECKED = -4,
	JSP_INPUT_DEFAULT_VALUE   = -5,
	JSP_INPUT_DISABLED        = -6,
	JSP_INPUT_FORM            = -7,
	JSP_INPUT_MAX_LENGTH      = -8,
	JSP_INPUT_NAME            = -9,
	JSP_INPUT_READONLY        = -10,
	JSP_INPUT_SELECTED_INDEX  = -11,
	JSP_INPUT_SIZE            = -12,
	JSP_INPUT_SRC             = -13,
	JSP_INPUT_TABINDEX        = -14,
	JSP_INPUT_TYPE            = -15,
	JSP_INPUT_VALUE           = -16,
};

static JSString *unicode_to_jsstring(JSContext *ctx, unicode_val_T u);
static unicode_val_T jsval_to_accesskey(JSContext *ctx, JS::MutableHandleValue hvp);
static struct form_state *input_get_form_state(JSContext *ctx, JSObject *jsinput);


static bool
input_get_property_accessKey(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	JSString *keystr;

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!link) {
		args.rval().setUndefined();
		return true;
	}

	if (!link->accesskey) {
		args.rval().set(JS_GetEmptyStringValue(ctx));
	} else {
		keystr = unicode_to_jsstring(ctx, link->accesskey);
		if (keystr) {
			args.rval().setString(keystr);
		}
		else
			return false;
	}
	return true;
}

static bool
input_set_property_accessKey(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	unicode_val_T accesskey;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

//	accesskey = jsval_to_accesskey(ctx, args[0]);

	size_t len;
	char16_t chr[2];

	accesskey = UCS_NO_CHAR;

	if (!args[0].isString()) {
		return false;
	}
	JSString *str = args[0].toString();

	len = JS_GetStringLength(str);

	/* This implementation ignores extra characters in the string.  */
	if (len < 1) {
		accesskey = 0;	/* which means no access key */
	} else if (len == 1) {
		JS_GetStringCharAt(ctx, str, 0, &chr[0]);
		if (!is_utf16_surrogate(chr[0])) {
			accesskey = chr[0];
		}
	} else {
		JS_GetStringCharAt(ctx, str, 1, &chr[1]);
		if (is_utf16_high_surrogate(chr[0])
			&& is_utf16_low_surrogate(chr[1])) {
			accesskey = join_utf16_surrogates(chr[0], chr[1]);
		}
	}
	if (accesskey == UCS_NO_CHAR) {
		JS_ReportErrorUTF8(ctx, "Invalid UTF-16 sequence");
		return false;
	}

	if (link) {
		link->accesskey = accesskey;
	}

	return true;
}

static bool
input_get_property_alt(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setString(JS_NewStringCopyZ(ctx, fc->alt));

	return true;
}

static bool
input_set_property_alt(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	mem_free_set(&fc->alt, stracpy(JS_EncodeString(ctx, args[0].toString())));

	return true;
}

static bool
input_get_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct form_state *fs;

	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */

	args.rval().setBoolean(fs->state);

	return true;
}

static bool
input_set_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO)
		return true;
	fs->state = args[0].toBoolean();

	return true;
}

static bool
input_get_property_defaultChecked(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setBoolean(fc->default_state);

	return true;
}

static bool
input_get_property_defaultValue(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME (bug 805): convert from the charset of the document */
	args.rval().setString(JS_NewStringCopyZ(ctx, fc->default_value));

	return true;
}

static bool
input_get_property_disabled(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	args.rval().setBoolean(fc->mode == FORM_MODE_DISABLED);

	return true;
}

static bool
input_set_property_disabled(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	fc->mode = (args[0].toBoolean() ? FORM_MODE_DISABLED
		: fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		: FORM_MODE_NORMAL);

	return true;
}

static bool
input_get_property_form(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::RootedObject parent_form(ctx, js::GetGlobalForObjectCrossCompartment(hobj));
	assert(JS_InstanceOf(ctx, parent_form, &form_class, NULL));
	if_assert_failed return false;

	args.rval().setObject(*parent_form);

	return true;
}

static bool
input_get_property_maxLength(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setInt32(fc->maxlength);

	return true;
}

static bool
input_set_property_maxLength(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	fc->maxlength = args[0].toInt32();

	return true;
}

static bool
input_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setString(JS_NewStringCopyZ(ctx, fc->name));

	return true;
}

/* @input_class.setProperty */
static bool
input_set_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);


	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	mem_free_set(&fc->name, stracpy(JS_EncodeString(ctx, args[0].toString())));

	return true;
}

static bool
input_get_property_readonly(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	args.rval().setBoolean(fc->mode == FORM_MODE_READONLY);

	return true;
}

/* @input_class.setProperty */
static bool
input_set_property_readonly(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	fc->mode = (args[0].toBoolean() ? FORM_MODE_READONLY
	                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
	                                                       : FORM_MODE_NORMAL);

	return true;
}

static bool
input_get_property_selectedIndex(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type == FC_SELECT) {
		args.rval().setInt32(fs->state);
	}
	else {
		args.rval().setUndefined();
	}

	return true;
}

/* @input_class.setProperty */
static bool
input_set_property_selectedIndex(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type == FC_SELECT) {
		int item = args[0].toInt32();

		if (item >= 0 && item < fc->nvalues) {
			fs->state = item;
			mem_free_set(&fs->value, stracpy(fc->values[item]));
		}
	}

	return true;
}

static bool
input_get_property_size(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setInt32(fc->size);

	return true;
}

static bool
input_get_property_src(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link && link->where_img) {
		args.rval().setString(JS_NewStringCopyZ(ctx, link->where_img));
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
input_set_property_src(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		mem_free_set(&link->where_img, stracpy(JS_EncodeString(ctx, args[0].toString())));
	}

	return true;
}

static bool
input_get_property_tabIndex(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		/* FIXME: This is WRONG. --pasky */
		args.rval().setInt32(link->number);
	} else {
		args.rval().setUndefined();
	}

	return true;
}

static bool
input_get_property_type(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	char *s = NULL;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	switch (fc->type) {
	case FC_TEXT: s = "text"; break;
	case FC_PASSWORD: s = "password"; break;
	case FC_FILE: s = "file"; break;
	case FC_CHECKBOX: s = "checkbox"; break;
	case FC_RADIO: s = "radio"; break;
	case FC_SUBMIT: s = "submit"; break;
	case FC_IMAGE: s = "image"; break;
	case FC_RESET: s = "reset"; break;
	case FC_BUTTON: s = "button"; break;
	case FC_HIDDEN: s = "hidden"; break;
	case FC_SELECT: s = "select"; break;
	default: INTERNAL("input_get_property() upon a non-input item."); break;
	}
	args.rval().setString(JS_NewStringCopyZ(ctx, s));

	return true;
}

static bool
input_get_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct form_state *fs;
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */

	args.rval().setString(JS_NewStringCopyZ(ctx, fs->value));

	return true;
}

static bool
input_set_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_FILE) {
		mem_free_set(&fs->value, stracpy(JS_EncodeString(ctx, args[0].toString())));
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
	}

	return true;
}

/* XXX: Some of those are marked readonly just because we can't change them
 * safely now. Changing default* values would affect all open instances of the
 * document, leading to a potential security risk. Changing size and type would
 * require re-rendering the document (TODO), tabindex would require renumbering
 * of all links and whatnot. --pasky */
static JSPropertySpec input_props[] = {
	JS_PSGS("accessKey",	input_get_property_accessKey, input_set_property_accessKey, JSPROP_ENUMERATE),
	JS_PSGS("alt",	input_get_property_alt, input_set_property_alt, JSPROP_ENUMERATE),
	JS_PSGS("checked",	input_get_property_checked, input_set_property_checked, JSPROP_ENUMERATE),
	JS_PSG("defaultChecked", input_get_property_defaultChecked, JSPROP_ENUMERATE),
	JS_PSG("defaultValue",input_get_property_defaultValue, JSPROP_ENUMERATE),
	JS_PSGS("disabled",	input_get_property_disabled, input_set_property_disabled, JSPROP_ENUMERATE),
	JS_PSG("form",	input_get_property_form, JSPROP_ENUMERATE),
	JS_PSGS("maxLength",	input_get_property_maxLength, input_set_property_maxLength, JSPROP_ENUMERATE),
	JS_PSGS("name",	input_get_property_name, input_set_property_name, JSPROP_ENUMERATE),
	JS_PSGS("readonly",	input_get_property_readonly, input_set_property_readonly, JSPROP_ENUMERATE),
	JS_PSGS("selectedIndex", input_get_property_selectedIndex, input_set_property_selectedIndex, JSPROP_ENUMERATE),
	JS_PSG("size",	input_get_property_size, JSPROP_ENUMERATE),
	JS_PSGS("src",	input_get_property_src, input_set_property_src,JSPROP_ENUMERATE),
	JS_PSG("tabindex",	input_get_property_tabIndex, JSPROP_ENUMERATE),
	JS_PSG("type",	input_get_property_type, JSPROP_ENUMERATE),
	JS_PSGS("value",	input_get_property_value, input_set_property_value, JSPROP_ENUMERATE),
	JS_PS_END
};

static bool input_blur(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool input_click(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool input_focus(JSContext *ctx, unsigned int argc, JS::Value *rval);
static bool input_select(JSContext *ctx, unsigned int argc, JS::Value *rval);

static const spidermonkeyFunctionSpec input_funcs[] = {
	{ "blur",	input_blur,	0 },
	{ "click",	input_click,	0 },
	{ "focus",	input_focus,	0 },
	{ "select",	input_select,	0 },
	{ NULL }
};

static struct form_state *
input_get_form_state(JSContext *ctx, JSObject *jsinput)
{
	JS::RootedObject r_jsinput(ctx, jsinput);
	struct form_state *fs = JS_GetInstancePrivate(ctx, r_jsinput,
						      &input_class,
						      NULL);

	if (!fs) return NULL;	/* detached */

	assert(fs->ecmascript_obj == jsinput);
	if_assert_failed return NULL;

	return fs;
}

/* @input_class.getProperty */
static bool
input_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	hvp.setUndefined();

	return true;
}

/* @input_class.setProperty */
static bool
input_set_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = hid.get();

	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	unicode_val_T accesskey;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL))
		return false;

	return true;
}

/* @input_funcs{"blur"} */
static bool
input_blur(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return true;
}

/* @input_funcs{"click"} */
static bool
input_click(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::Value val;
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::RootedObject hobj(ctx, JS_THIS_OBJECT(ctx, rval));
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &input_class, &args)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return true;

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);
	else
		print_screen_status(ses);

	args.rval().setBoolean(false);

	return true;
}

/* @input_funcs{"focus"} */
static bool
input_focus(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::Value val;
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::RootedObject hobj(ctx, JS_THIS_OBJECT(ctx, rval));
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &input_class, &args)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) return false; /* detached */

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return true;

	jump_to_link_number(ses, doc_view, linknum);

	args.rval().setBoolean(false);
	return true;
}

/* @input_funcs{"select"} */
static bool
input_select(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	return true;
}

static JSObject *
get_input_object(JSContext *ctx, struct form_state *fs)
{
	JSObject *jsinput = fs->ecmascript_obj;

	if (jsinput) {
		JS::RootedObject r_jsinput(ctx, jsinput);
		/* This assumes JS_GetInstancePrivate cannot GC.  */
		assert(JS_GetInstancePrivate(ctx, r_jsinput,
					     &input_class, NULL)
		       == fs);
		if_assert_failed return NULL;

		return jsinput;
	}

	/* jsform ('form') is input's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */
	jsinput = JS_NewObject(ctx, &input_class);
	if (!jsinput)
		return NULL;

	JS::RootedObject r_jsinput(ctx, jsinput);

	JS_DefineProperties(ctx, r_jsinput, (JSPropertySpec *) input_props);
	spidermonkey_DefineFunctions(ctx, jsinput, input_funcs);

	JS_SetPrivate(jsinput, fs); /* to @input_class */
	fs->ecmascript_obj = jsinput;
	return jsinput;
}

static void
input_finalize(JSFreeOp *op, JSObject *jsinput)
{
	struct form_state *fs = JS_GetPrivate(jsinput);

	if (fs) {
		/* If this assertion fails, leave fs->ecmascript_obj
		 * unchanged, because it may point to a different
		 * JSObject whose private pointer will later have to
		 * be updated to avoid crashes.  */
		assert(fs->ecmascript_obj == jsinput);
		if_assert_failed return;

		fs->ecmascript_obj = NULL;
		/* No need to JS_SetPrivate, because jsinput is being
		 * destroyed.  */
	}
}

void
spidermonkey_detach_form_state(struct form_state *fs)
{
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
//
		JS_SetPrivate(jsinput, NULL);
		fs->ecmascript_obj = NULL;
	}
}

void
spidermonkey_moved_form_state(struct form_state *fs)
{
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


static struct form_view *form_get_form_view(JSContext *ctx, JSObject *jsform, JS::Value *argv);
static bool form_elements_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);

static JSClassOps form_elements_ops = {
	JS_PropertyStub, nullptr,
	form_elements_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
};

/* Each @form_elements_class object must have a @form_class parent.  */
static JSClass form_elements_class = {
	"elements",
	JSCLASS_HAS_PRIVATE,
	&form_elements_ops
};

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

static bool
form_elements_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	jsid id = hid.get();

	JS::Value idval;
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;

	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) {
		return false;
	}
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
//	form_view = form_get_form_view(ctx, nullptr, /*parent_form*/ NULL);
	if (!form_view) return false; /* detached */
	form = find_form_by_form_view(document, form_view);

	if (JSID_IS_STRING(id)) {
		JS::RootedValue r_idval(ctx, idval);
		JS_IdToValue(ctx, id, &r_idval);
		char *string = JS_EncodeString(ctx, r_idval.toString());

		std::string test = string;
		if (test == "item" || test == "namedItem") {
			return JS_PropertyStub(ctx, hobj, hid, hvp);
		}

		form_elements_namedItem2(ctx, hobj, string, hvp);
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
		return false;
	}

	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
//	form_view = form_get_form_view(ctx, nullptr, /*parent_form*/ NULL);
	if (!form_view) return false; /* detached */

	form = find_form_by_form_view(document, form_view);
	args.rval().setInt32(list_size(&form->items));

	return true;
}


/* @form_elements_funcs{"item"} */
static bool
form_elements_item(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
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
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct el_form_control *fc;
	int counter = -1;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);

//	form_view = form_get_form_view(ctx, nullptr/*parent_form*/, NULL);

	if (!form_view) return false; /* detached */
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
	JS::Value val;
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	JS::RootedValue rval(ctx, val);


//	JS::Value *argv = JS_ARGV(ctx, rval);
	char *string = JS_EncodeString(ctx, args[0].toString());
	bool ret = form_elements_namedItem2(ctx, hobj, string, &rval);
	args.rval().set(rval);
//	JS_SET_RVAL(ctx, rval, val);
	return ret;
}

static bool
form_elements_namedItem2(JSContext *ctx, JS::HandleObject hobj, char *string, JS::MutableHandleValue hvp)
{
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct el_form_control *fc;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!*string) {
		return true;
	}

	if (!JS_InstanceOf(ctx, hobj, &form_elements_class, NULL)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;

	form_view = JS_GetInstancePrivate(ctx, hobj, &form_elements_class, nullptr);
//	form_view = form_get_form_view(ctx, nullptr, /*parent_form*/ NULL);
	if (!form_view) return false; /* detached */
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
form_get_form_view(JSContext *ctx, JSObject *jsform, JS::Value *argv)
{
	JS::RootedObject r_jsform(ctx, jsform);
	struct form_view *fv = JS_GetInstancePrivate(ctx, r_jsform,
						     &form_class,
						     NULL);

	if (!fv) return NULL;	/* detached */

	assert(fv->ecmascript_obj == jsform);
	if_assert_failed return NULL;
	
	return fv;
}

/* @form_class.getProperty */
static bool
form_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
	ELINKS_CAST_PROP_PARAMS
	jsid id = hid.get();
	/* DBG("doc %p %s\n", parent_doc, JS_GetStringBytes(JS_ValueToString(ctx, OBJECT_TO_JSVAL(parent_doc)))); */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);
	args.rval().setString(JS_NewStringCopyZ(ctx, form->action));

	return true;
}

static bool
form_set_property_action(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	char *string;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	string = stracpy(JS_EncodeString(ctx, args[0].toString()));
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct form_view *fv;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */

	/* jsform ('form') is form_elements' parent; who knows is that's correct */
	JSObject *jsform_elems = JS_NewObjectWithGivenProto(ctx, &form_elements_class, hobj);
	JS::RootedObject r_jsform_elems(ctx, jsform_elems);

	JS_DefineProperties(ctx, r_jsform_elems, (JSPropertySpec *) form_elements_props);
	spidermonkey_DefineFunctions(ctx, jsform_elems,
				     form_elements_funcs);
	JS_SetPrivate(jsform_elems, fv);

	args.rval().setObject(*jsform_elems);

	return true;
}

static bool
form_get_property_encoding(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	char *string;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	string = JS_EncodeString(ctx, args[0].toString());
	if (!c_strcasecmp(string, "application/x-www-form-urlencoded")) {
		form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
		                                               : FORM_METHOD_POST;
	} else if (!c_strcasecmp(string, "multipart/form-data")) {
		form->method = FORM_METHOD_POST_MP;
	} else if (!c_strcasecmp(string, "text/plain")) {
		form->method = FORM_METHOD_POST_TEXT_PLAIN;
	}

	return true;
}

static bool
form_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	args.rval().setInt32(list_size(&form->items));

	return true;
}

static bool
form_get_property_method(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	char *string;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	string = JS_EncodeString(ctx, args[0].toString());
	if (!c_strcasecmp(string, "GET")) {
		form->method = FORM_METHOD_GET;
	} else if (!c_strcasecmp(string, "POST")) {
		form->method = FORM_METHOD_POST;
	}

	return true;
}

static bool
form_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	args.rval().setString(JS_NewStringCopyZ(ctx, form->name));

	return true;
}

/* @form_class.setProperty */
static bool
form_set_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);
	mem_free_set(&form->name, stracpy(JS_EncodeString(ctx, args[0].toString())));

	return true;
}

static bool
form_get_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);
	args.rval().setString(JS_NewStringCopyZ(ctx, form->target));

	return true;
}

static bool
form_set_property_target(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &form_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, hobj, NULL);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);
	mem_free_set(&form->target, stracpy(JS_EncodeString(ctx, args[0].toString())));

	return true;
}


/* @form_funcs{"reset"} */
static bool
form_reset(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
	JS::Value val;
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JSObject *obj = JS_THIS_OBJECT(ctx, rval);
	JS::RootedObject hobj(ctx, obj);
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &form_class, &args)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
///	fv = form_get_form_view(ctx, obj, argv);
	fv = form_get_form_view(ctx, obj, rval);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

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
	JS::Value val;
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JSObject *obj = JS_THIS_OBJECT(ctx, rval);
	JS::RootedObject hobj(ctx, obj);
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	struct form_view *fv;
	struct form *form;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &form_class, &args)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	ses = doc_view->session;
//	fv = form_get_form_view(ctx, obj, argv);
	fv = form_get_form_view(ctx, obj, rval);
	if (!fv) return false; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);
	submit_given_form(ses, doc_view, form, 0);

	args.rval().setBoolean(false);

	return true;
}

JSObject *
get_form_object(JSContext *ctx, JSObject *jsdoc, struct form_view *fv)
{
	JSObject *jsform = fv->ecmascript_obj;

	if (jsform) {
		JS::RootedObject r_jsform(ctx, jsform);
		/* This assumes JS_GetInstancePrivate cannot GC.  */
		assert(JS_GetInstancePrivate(ctx, r_jsform,
					     &form_class, NULL)
		       == fv);
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

	JS_SetPrivate(jsform, fv); /* to @form_class */
	fv->ecmascript_obj = jsform;

	return jsform;
}

static void
form_finalize(JSFreeOp *op, JSObject *jsform)
{
	struct form_view *fv = JS_GetPrivate(jsform);

	if (fv) {
		/* If this assertion fails, leave fv->ecmascript_obj
		 * unchanged, because it may point to a different
		 * JSObject whose private pointer will later have to
		 * be updated to avoid crashes.  */
		assert(fv->ecmascript_obj == jsform);
		if_assert_failed return;

		fv->ecmascript_obj = NULL;
		/* No need to JS_SetPrivate, because the object is
		 * being destroyed.  */
	}
}

void
spidermonkey_detach_form_view(struct form_view *fv)
{
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


static bool forms_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp);
static bool forms_get_property_length(JSContext *ctx, unsigned int argc, JS::Value *vp);

JSClassOps forms_ops = {
	JS_PropertyStub, nullptr,
	forms_get_property, JS_StrictPropertyStub,
	nullptr, nullptr, nullptr, nullptr
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
	struct form *form;

	if (!*string)
		return;

	foreach (form, doc_view->document->forms) {
		if (form->name && !c_strcasecmp(string, form->name)) {
			hvp.setObject(*get_form_object(ctx, nullptr,
					find_form_view(doc_view, form)));
			break;
		}
	}
}

/* @forms_class.getProperty */
static bool
forms_get_property(JSContext *ctx, JS::HandleObject hobj, JS::HandleId hid, JS::MutableHandleValue hvp)
{
//	jsid id = hid.get();

	JS::Value idval;
//	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct document_view *doc_view;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL))
		return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (JSID_IS_STRING(hid)) {
		char *string = jsid_to_string(ctx, hid);
		std::string test = string;

		if (test == "item" || test == "namedItem") {
			return JS_PropertyStub(ctx, hobj, hid, hvp);
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
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL))
		return false;

	vs = interpreter->vs;
	if (!vs) {
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
//	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	struct view_state *vs;
	struct form_view *fv;
	int counter = -1;

	if (!JS_InstanceOf(ctx, hobj, &forms_class, NULL))
		return false;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	vs = interpreter->vs;

	hvp.setUndefined();

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			hvp.setObject(*get_form_object(ctx, nullptr, fv));
			break;
		}
	}

	return true;
}

/* @forms_funcs{"namedItem"} */
static bool
forms_namedItem(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
	JS::Value val;
//	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

//	JS::Value *argv = JS_ARGV(ctx, rval);
	struct view_state *vs;
	struct document_view *doc_view;
	JSCompartment *comp = js::GetContextCompartment(ctx);

	if (!comp) {
		return false;
	}

	struct ecmascript_interpreter *interpreter = JS_GetCompartmentPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &forms_class, &args)) return false;

	vs = interpreter->vs;
	doc_view = vs->doc_view;

	if (argc != 1)
		return true;

	char *string = JS_EncodeString(ctx, args[0].toString());

	JS::RootedValue rval(ctx, val);
	rval.setNull();

	find_form_by_name(ctx, doc_view, string, &rval);
	args.rval().set(rval.get());

	return true;
}


static JSString *
unicode_to_jsstring(JSContext *ctx, unicode_val_T u)
{
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
