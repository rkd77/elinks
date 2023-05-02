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

/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static void input_finalize(JS::GCContext *op, JSObject *obj);

static JSClassOps input_ops = {
	nullptr,  // addProperty
	nullptr,  // deleteProperty
	nullptr,  // enumerate
	nullptr,  // newEnumerate
	nullptr,  // resolve
	nullptr,  // mayResolve
	input_finalize,  // finalize
	nullptr,  // call
	nullptr,  // construct
	JS_GlobalObjectTraceHook
};

/* Each @input_class object must have a @form_class parent.  */
static JSClass input_class = {
	"input", /* here, we unleash ourselves */
	JSCLASS_HAS_RESERVED_SLOTS(1),	/* struct form_state *, or NULL if detached */
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

static struct form_state *input_get_form_state(JSContext *ctx, JSObject *jsinput);


static bool
input_get_property_accessKey(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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

	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
		else {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
			return false;
		}
	}
	return true;
}

static bool
input_set_property_accessKey(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setString(JS_NewStringCopyZ(ctx, fc->alt));

	return true;
}

static bool
input_set_property_alt(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	mem_free_set(&fc->alt, jsval_to_string(ctx, args[0]));

	return true;
}

static bool
input_get_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct form_state *fs;

	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}

	args.rval().setBoolean(fs->state);

	return true;
}

static bool
input_set_property_checked(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setBoolean(fc->default_state);

	return true;
}

static bool
input_get_property_defaultValue(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	JS::RootedObject parent_form(ctx, JS::GetNonCCWObjectGlobal(hobj));
	assert(JS_InstanceOf(ctx, parent_form, &form_class, NULL));
	if_assert_failed {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	args.rval().setObject(*parent_form);

	return true;
}

static bool
input_get_property_maxLength(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setInt32(fc->maxlength);

	return true;
}

static bool
input_set_property_maxLength(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	fc->maxlength = args[0].toInt32();

	return true;
}

static bool
input_get_property_name(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	vs = interpreter->vs;
	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	mem_free_set(&fc->name, jsval_to_string(ctx, args[0]));

	return true;
}

static bool
input_get_property_readonly(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	args.rval().setInt32(fc->size);

	return true;
}

static bool
input_get_property_src(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	if (!vs) {
		return false;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		mem_free_set(&link->where_img, jsval_to_string(ctx, args[0]));
	}

	return true;
}

static bool
input_get_property_tabIndex(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	const char *s = NULL;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct form_state *fs;
	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}

	args.rval().setString(JS_NewStringCopyZ(ctx, fs->value));

	return true;
}

static bool
input_set_property_value(JSContext *ctx, unsigned int argc, JS::Value *vp)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::CallArgs args = CallArgsFromVp(argc, vp);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
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
	if (!JS_InstanceOf(ctx, hobj, &input_class, NULL)) {
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
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_FILE) {
		mem_free_set(&fs->value, jsval_to_string(ctx, args[0]));
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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs = JS::GetMaybePtrFromReservedSlot<struct form_state>(jsinput, 0);

	if (!fs) return NULL;	/* detached */

	assert(fs->ecmascript_obj == jsinput);
	if_assert_failed return NULL;

	return fs;
}

/* @input_funcs{"blur"} */
static bool
input_blur(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return true;
}

/* @input_funcs{"click"} */
static bool
input_click(JSContext *ctx, unsigned int argc, JS::Value *rval)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &input_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}

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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JS::RootedObject parent_form(ctx);	/* instance of @form_class */
	JS::RootedObject parent_doc(ctx);	/* instance of @document_class */
	JS::CallArgs args = JS::CallArgsFromVp(argc, rval);
	JS::RootedObject hobj(ctx, &args.thisv().toObject());

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	JS::Realm *comp = js::GetContextRealm(ctx);

	if (!comp) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS::GetRealmPrivate(comp);

	if (!JS_InstanceOf(ctx, hobj, &input_class, &args)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false;
	}

	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = input_get_form_state(ctx, hobj);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return false; /* detached */
	}

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
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	return true;
}

JSObject *
get_input_object(JSContext *ctx, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JSObject *jsinput = (JSObject *)fs->ecmascript_obj;

	if (jsinput) {
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

	JS::SetReservedSlot(jsinput, 0, JS::PrivateValue(fs)); /* to @input_class */
	fs->ecmascript_obj = jsinput;

	return jsinput;
}

static void
input_finalize(JS::GCContext *op, JSObject *jsinput)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs = JS::GetMaybePtrFromReservedSlot<struct form_state>(jsinput, 0);

	if (fs) {
		/* If this assertion fails, leave fs->ecmascript_obj
		 * unchanged, because it may point to a different
		 * JSObject whose private pointer will later have to
		 * be updated to avoid crashes.  */
		assert(fs->ecmascript_obj == jsinput);
		if_assert_failed return;

		fs->ecmascript_obj = NULL;
		/* No need to JS::SetPrivate, because jsinput is being
		 * destroyed.  */
	}
}
