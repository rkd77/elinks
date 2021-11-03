/* The SpiderMonkey window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

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
#include "ecmascript/quickjs.h"
//#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/form.h"
#include "ecmascript/quickjs/forms.h"
#include "ecmascript/quickjs/input.h"
#include "ecmascript/quickjs/window.h"
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

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_input_class_id;

/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static struct form_state *js_input_get_form_state(JSContext *ctx, JSValueConst jsinput);

static JSValue
js_input_get_property_accessKey(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!link) {
		return JS_UNDEFINED;
	}

	if (!link->accesskey) {
		return JS_NewStringLen(ctx, "", 0);
	} else {
		const char *keystr = encode_utf8(link->accesskey);
		if (keystr) {
			return JS_NewString(ctx, keystr);
		} else {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
			return JS_UNDEFINED;
		}
	}
	return JS_UNDEFINED;
}

static JSValue
js_input_set_property_accessKey(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	unicode_val_T accesskey;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	accesskey = UCS_NO_CHAR;

	if (!JS_IsString(val)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	const char *str;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}

	char *string = str;
	accesskey = utf8_to_unicode(&string, str + len);
	JS_FreeCString(ctx, str);

	if (link) {
		link->accesskey = accesskey;
	}

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_alt(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	return JS_NewString(ctx, fc->alt);
}

static JSValue
js_input_set_property_alt(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	const char *str;
	char *string;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}
	string = stracpy(str);
	JS_FreeCString(ctx, str);
	mem_free_set(&fc->alt, string);

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_checked(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs;
	fs = js_input_get_form_state(ctx, this_val);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}

	return JS_NewBool(ctx, fs->state);
}

static JSValue
js_input_set_property_checked(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO) {
		return JS_UNDEFINED;
	}
	fs->state = JS_ToBool(ctx, val);

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_defaultChecked(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	return JS_NewBool(ctx, fc->default_state);
}

static JSValue
js_input_get_property_defaultValue(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME (bug 805): convert from the charset of the document */
	return JS_NewString(ctx, fc->default_value);
}

static JSValue
js_input_get_property_disabled(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	return JS_NewBool(ctx, fc->mode == FORM_MODE_DISABLED);
}

static JSValue
js_input_set_property_disabled(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	fc->mode = (JS_ToBool(ctx, val) ? FORM_MODE_DISABLED
		: fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		: FORM_MODE_NORMAL);

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_form(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
#if 0
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
#endif
	// TODO
	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_maxLength(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	return JS_NewInt32(ctx, fc->maxlength);
}

static JSValue
js_input_set_property_maxLength(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	JS_ToInt32(ctx, &fc->maxlength, val);

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_name(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	return JS_NewString(ctx, fc->name);
}

/* @input_class.setProperty */
static JSValue
js_input_set_property_name(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	const char *str;
	char *string;
	size_t len;
	str = JS_ToCStringLen(ctx, &len, val);

	if (!str) {
		return JS_EXCEPTION;
	}

	string = stracpy(str);
	JS_FreeCString(ctx, str);
	mem_free_set(&fc->name, string);

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_readonly(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	return JS_NewBool(ctx, fc->mode == FORM_MODE_READONLY);
}

/* @input_class.setProperty */
static JSValue
js_input_set_property_readonly(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	fc->mode = (JS_ToBool(ctx, val) ? FORM_MODE_READONLY
	                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
	                                                       : FORM_MODE_NORMAL);

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_selectedIndex(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type == FC_SELECT) {
		return JS_NewInt32(ctx, fs->state);
	} else {
		return JS_UNDEFINED;
	}
}

/* @input_class.setProperty */
static JSValue
js_input_set_property_selectedIndex(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type == FC_SELECT) {
		int item;

		JS_ToInt32(ctx, &item, val);

		if (item >= 0 && item < fc->nvalues) {
			fs->state = item;
			mem_free_set(&fs->value, stracpy(fc->values[item]));
		}
	}

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_size(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	return JS_NewInt32(ctx, fc->size);
}

static JSValue
js_input_get_property_src(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link && link->where_img) {
		return JS_NewString(ctx, link->where_img);
	} else {
		return JS_UNDEFINED;
	}
}

static JSValue
js_input_set_property_src(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
		return JS_UNDEFINED;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		const char *str;
		char *string;
		size_t len;
		str = JS_ToCStringLen(ctx, &len, val);

		if (!str) {
			return JS_EXCEPTION;
		}

		string = stracpy(str);
		JS_FreeCString(ctx, str);

		mem_free_set(&link->where_img, string);
	}

	return JS_UNDEFINED;
}

static JSValue
js_input_get_property_tabIndex(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		/* FIXME: This is WRONG. --pasky */
		return JS_NewInt32(ctx, link->number);
	} else {
		return JS_UNDEFINED;
	}
}

static JSValue
js_input_get_property_type(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	char *s = NULL;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
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

	return JS_NewString(ctx, s);
}

static JSValue
js_input_get_property_value(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}

	return JS_NewString(ctx, fs->value);
}

static JSValue
js_input_set_property_value(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_FILE) {
		const char *str, *string;
		size_t len;

		str = JS_ToCStringLen(ctx, &len, val);

		if (!str) {
			return JS_EXCEPTION;
		}

		string = stracpy(str);
		JS_FreeCString(ctx, str);

		mem_free_set(&fs->value, string);
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
	}

	return JS_UNDEFINED;
}

#if 0
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
#endif

#if 0
static const spidermonkeyFunctionSpec input_funcs[] = {
	{ "blur",	input_blur,	0 },
	{ "click",	input_click,	0 },
	{ "focus",	input_focus,	0 },
	{ "select",	input_select,	0 },
	{ NULL }
};
#endif

static struct form_state *
js_input_get_form_state(JSContext *ctx, JSValueConst jsinput)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs = JS_GetOpaque(jsinput, js_input_class_id);

	if (!fs) return NULL;	/* detached */

//	assert(fs->ecmascript_obj == jsinput);
//	if_assert_failed return NULL;

	return fs;
}

/* @input_funcs{"blur"} */
static JSValue
js_input_blur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return JS_UNDEFINED;
}

/* @input_funcs{"click"} */
static JSValue
js_input_click(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0) {
		return JS_UNDEFINED;
	}

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH) {
		refresh_view(ses, doc_view, 0);
	} else {
		print_screen_status(ses);
	}

	return JS_FALSE;
}

/* @input_funcs{"focus"} */
static JSValue
js_input_focus(JSContext *ctx, JSValueConst this_val, unsigned int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct ecmascript_interpreter *interpreter = JS_GetContextOpaque(ctx);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0) {
		return JS_UNDEFINED;
	}

	jump_to_link_number(ses, doc_view, linknum);

	return JS_FALSE;
}

/* @input_funcs{"select"} */
static JSValue
js_input_select(JSContext *ctx, JSValueConst this_val, unsigned int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	return JS_UNDEFINED;
}

JSValue
getInput(JSContext *ctx, struct form_state *fs)
{
	// TODO
	return JS_NULL;
}

JSValue
js_get_input_object(JSContext *ctx, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	JSValue jsinput = fs->ecmascript_obj;

	if (!JS_IsNull(jsinput)) {
		assert(JS_GetOpaque(jsinput, js_input_class_id) == fs);
		if_assert_failed return JS_NULL;

		return jsinput;
	}

	/* jsform ('form') is input's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */

	return getInput(ctx, fs);
#if 0
	jsinput = JS_NewObject(ctx, &input_class);
	if (!jsinput)
		return NULL;

	JS::RootedObject r_jsinput(ctx, jsinput);

	JS_DefineProperties(ctx, r_jsinput, (JSPropertySpec *) input_props);
	spidermonkey_DefineFunctions(ctx, jsinput, input_funcs);

	JS_SetPrivate(jsinput, fs); /* to @input_class */
	fs->ecmascript_obj = jsinput;

	return jsinput;
#endif
}

static const JSCFunctionListEntry js_input_proto_funcs[] = {
	JS_CGETSET_DEF("accessKey",	js_input_get_property_accessKey, js_input_set_property_accessKey),
	JS_CGETSET_DEF("alt",	js_input_get_property_alt, js_input_set_property_alt),
	JS_CGETSET_DEF("checked",	js_input_get_property_checked, js_input_set_property_checked),
	JS_CGETSET_DEF("defaultChecked", js_input_get_property_defaultChecked, nullptr),
	JS_CGETSET_DEF("defaultValue",js_input_get_property_defaultValue, nullptr),
	JS_CGETSET_DEF("disabled",	js_input_get_property_disabled, js_input_set_property_disabled),
	JS_CGETSET_DEF("form",	js_input_get_property_form, nullptr),
	JS_CGETSET_DEF("maxLength",	js_input_get_property_maxLength, js_input_set_property_maxLength),
	JS_CGETSET_DEF("name",	js_input_get_property_name, js_input_set_property_name),
	JS_CGETSET_DEF("readonly",	js_input_get_property_readonly, js_input_set_property_readonly),
	JS_CGETSET_DEF("selectedIndex", js_input_get_property_selectedIndex, js_input_set_property_selectedIndex),
	JS_CGETSET_DEF("size",	js_input_get_property_size, nullptr),
	JS_CGETSET_DEF("src",	js_input_get_property_src, js_input_set_property_src),
	JS_CGETSET_DEF("tabindex",	js_input_get_property_tabIndex, nullptr),
	JS_CGETSET_DEF("type",	js_input_get_property_type, nullptr),
	JS_CGETSET_DEF("value",	js_input_get_property_value, js_input_set_property_value),
	JS_CFUNC_DEF("blur", 0 , js_input_blur),
	JS_CFUNC_DEF("click", 0 , js_input_click),
	JS_CFUNC_DEF("focus", 0 , js_input_focus),
	JS_CFUNC_DEF("select", 0 , js_input_select),
};

static JSClassDef js_input_class = {
	"input",
};

static JSValue
js_input_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_input_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	return obj;

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_input_init(JSContext *ctx, JSValue global_obj)
{
	JSValue input_proto, input_class;

	/* create the input class */
	JS_NewClassID(&js_input_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_input_class_id, &js_input_class);

	input_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, input_proto, js_input_proto_funcs, countof(js_input_proto_funcs));

	input_class = JS_NewCFunction2(ctx, js_input_ctor, "input", 0, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, input_class, input_proto);
	JS_SetClassProto(ctx, js_input_class_id, input_proto);

	JS_SetPropertyStr(ctx, global_obj, "input", input_proto);
	return 0;
}

#if 0
static void
input_finalize(JSFreeOp *op, JSObject *jsinput)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
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
#endif