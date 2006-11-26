/* The SpiderMonkey ECMAScript backend. */
/* $Id: spidermonkey.c,v 1.147.2.8 2005/04/06 08:59:38 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* For wild SpiderMonkey installations. */
#ifdef CONFIG_BEOS
#define XP_BEOS
#elif CONFIG_OS2
#define XP_OS2
#elif CONFIG_RISCOS
#error Out of luck, buddy!
#elif CONFIG_UNIX
#define XP_UNIX
#elif CONFIG_WIN32
#define XP_WIN
#endif

#include <jsapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "lowlevel/sysname.h"
#include "osdep/newwin.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "sched/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"



/*** Global methods */


/* TODO? Are there any which need to be implemented? */



/*** Classes */

enum prop_type {
	JSPT_UNDEF,
	JSPT_INT,
	JSPT_DOUBLE,
	JSPT_STRING,
	JSPT_ASTRING,
	JSPT_BOOLEAN,
	JSPT_OBJECT,
};

struct jsval_property {
	enum prop_type type;
	union {
		int boolean;
		int number;
		jsdouble floatnum;
		JSObject *object;
		unsigned char *string;
	} value;
};

static void
set_prop_undef(struct jsval_property *prop)
{
	memset(prop, 'J', sizeof(struct jsval_property)); /* Active security ;) */
	prop->type = JSPT_UNDEF;
}

static void
set_prop_object(struct jsval_property *prop, JSObject *object)
{
	prop->value.object = object;
	prop->type = JSPT_OBJECT;
}

static void
set_prop_boolean(struct jsval_property *prop, int boolean)
{
	prop->value.boolean = boolean;
	prop->type = JSPT_BOOLEAN;
}

static void
set_prop_string(struct jsval_property *prop, unsigned char *string)
{
	prop->value.string = string;
	prop->type = JSPT_STRING;
}

static void
set_prop_astring(struct jsval_property *prop, unsigned char *string)
{
	prop->value.string = string;
	prop->type = JSPT_ASTRING;
}

static void
set_prop_int(struct jsval_property *prop, int number)
{
	prop->value.number = number;
	prop->type = JSPT_INT;
}

#if 0 /* not yet used. */
static void
set_prop_double(struct jsval_property *prop, jsdouble floatnum)
{
	prop->value.floatnum = floatnum;
	prop->type = JSPT_DOUBLE;
}
#endif

static void
value_to_jsval(JSContext *ctx, jsval *vp, struct jsval_property *prop)
{
	switch (prop->type) {
	case JSPT_STRING:
	case JSPT_ASTRING:
		if (!prop->value.string) {
			*vp = JSVAL_NULL;
			break;
		}
		*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(ctx, prop->value.string));
		if (prop->type == JSPT_ASTRING)
			mem_free(prop->value.string);
		break;

	case JSPT_BOOLEAN:
		*vp = BOOLEAN_TO_JSVAL(prop->value.boolean);
		break;

	case JSPT_DOUBLE:
		*vp = DOUBLE_TO_JSVAL(prop->value.floatnum);
		break;

	case JSPT_INT:
		*vp = INT_TO_JSVAL(prop->value.number);
		break;

	case JSPT_OBJECT:
		*vp = OBJECT_TO_JSVAL(prop->value.object);
		break;

	case JSPT_UNDEF:
	default:
		*vp = JSVAL_NULL;
		break;
	}
}

union jsval_union {
	jsint boolean;
	jsdouble *number;
	unsigned char *string;
};

static void
jsval_to_value(JSContext *ctx, jsval *vp, JSType type, union jsval_union *var)
{
	jsval val;

	if (JS_ConvertValue(ctx, *vp, type, &val) == JS_FALSE) {
		switch (type) {
		case JSTYPE_BOOLEAN:
			var->boolean = JS_FALSE;
			break;
		case JSTYPE_NUMBER:
			var->number = NULL;
			break;
		case JSTYPE_STRING:
			var->string = NULL;
			break;
		case JSTYPE_VOID:
		case JSTYPE_OBJECT:
		case JSTYPE_FUNCTION:
		case JSTYPE_LIMIT:
		default:
			INTERNAL("Invalid type %d in jsval_to_value()", type);
			break;
		}
		return;
	}

	switch (type) {
	case JSTYPE_BOOLEAN:
		var->boolean = JSVAL_TO_BOOLEAN(val);
		break;
	case JSTYPE_NUMBER:
		var->number = JSVAL_TO_DOUBLE(val);
		break;
	case JSTYPE_STRING:
		var->string = JS_GetStringBytes(JS_ValueToString(ctx, val));
		break;
	case JSTYPE_VOID:
	case JSTYPE_OBJECT:
	case JSTYPE_FUNCTION:
	case JSTYPE_LIMIT:
	default:
		INTERNAL("Invalid type %d in jsval_to_value()", type);
		break;
	}
}



static JSBool window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass window_class = {
	"window",
	JSCLASS_HAS_PRIVATE,	/* struct view_state * */
	JS_PropertyStub, JS_PropertyStub,
	window_get_property, window_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum window_prop {
	JSP_WIN_CLOSED,
	JSP_WIN_PARENT,
	JSP_WIN_SELF,
	JSP_WIN_TOP,
};
/* "location" is special because we need to simulate "location.href"
 * when the code is asking directly for "location". We do not register
 * it as a "known" property since that was yielding strange bugs
 * (SpiderMonkey was still asking us about the "location" string after
 * assigning to it once), instead we do just a little string
 * comparing. */
static const JSPropertySpec window_props[] = {
	{ "closed",	JSP_WIN_CLOSED,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "parent",	JSP_WIN_PARENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "self",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "top",	JSP_WIN_TOP,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "window",	JSP_WIN_SELF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};


static JSObject *
try_resolve_frame(struct document_view *doc_view, unsigned char *id)
{
	struct session *ses = doc_view->session;
	struct frame *target;

	assert(ses);
	target = ses_find_frame(ses, id);
	if (!target) return NULL;
	if (target->vs.ecmascript_fragile)
		ecmascript_reset_state(&target->vs);
	if (!target->vs.ecmascript) return NULL;
	return JS_GetGlobalObject(target->vs.ecmascript->backend_data);
}

#if 0
static struct frame_desc *
find_child_frame(struct document_view *doc_view, struct frame_desc *tframe)
{
	struct frameset_desc *frameset = doc_view->document->frame_desc;
	int i;

	if (!frameset)
		return NULL;

	for (i = 0; i < frameset->n; i++) {
		struct frame_desc *frame = &frameset->frame_desc[i];

		if (frame == tframe)
			return frame;
	}

	return NULL;
}
#endif

/* @window_class.getProperty */
static JSBool
window_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs;
	struct jsval_property prop;

	vs = JS_GetPrivate(ctx, obj); /* from @window_class */

	set_prop_undef(&prop);

	/* No need for special window.location measurements - when
	 * location is then evaluated in string context, toString()
	 * is called which we overrode for that class below, so
	 * everything's fine. */
	if (JSVAL_IS_STRING(id)) {
		struct document_view *doc_view = vs->doc_view;
		JSObject *obj;
		union jsval_union v;

		jsval_to_value(ctx, &id, JSTYPE_STRING, &v);
		obj = try_resolve_frame(doc_view, v.string);
		/* TODO: Try other lookups (mainly element lookup) until
		 * something yields data. */
		if (!obj) return JS_TRUE;
		set_prop_object(&prop, obj);
		goto convert;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_WIN_CLOSED:
		/* TODO: It will be a major PITA to implement this properly.
		 * Well, perhaps not so much if we introduce reference tracking
		 * for (struct session)? Still... --pasky */
		set_prop_boolean(&prop, 0);
		break;
	case JSP_WIN_SELF:
		set_prop_object(&prop, obj);
		break;
	case JSP_WIN_PARENT:
		/* XXX: It would be nice if the following worked, yes.
		 * The problem is that we get called at the point where
		 * document.frame properties are going to be mostly NULL.
		 * But the problem is deeper because at that time we are
		 * yet building scrn_frames so our parent might not be there
		 * yet (XXX: is this true?). The true solution will be to just
		 * have struct document_view *(document_view.parent). --pasky */
		/* FIXME: So now we alias window.parent to window.top, which is
		 * INCORRECT but works for the most common cases of just two
		 * frames. Better something than nothing. */
#if 0
	{
		/* This is horrible. */
		struct document_view *doc_view = vs->doc_view;
		struct session *ses = doc_view->session;
		struct frame_desc *frame = doc_view->document->frame;

		if (!ses->doc_view->document->frame_desc) {
			INTERNAL("Looking for parent but there're no frames.");
			break;
		}
		assert(frame);
		doc_view = ses->doc_view;
		if (find_child_frame(doc_view, frame))
			goto found_parent;
		foreach (doc_view, ses->scrn_frames) {
			if (find_child_frame(doc_view, frame))
				goto found_parent;
		}
		INTERNAL("Cannot find frame %s parent.",doc_view->name);
		break;

found_parent:
		some_domain_security_check();
		if (doc_view->vs.ecmascript_fragile)
			ecmascript_reset_state(&doc_view->vs);
		assert(doc_view->ecmascript);
		set_prop_object(&prop, JS_GetGlobalObject(doc_view->ecmascript->backend_data));
		break;
	}
#endif
	case JSP_WIN_TOP:
	{
		struct document_view *doc_view = vs->doc_view;
		struct document_view *top_view = doc_view->session->doc_view;
		JSObject *newjsframe;

		assert(top_view && top_view->vs);
		if (top_view->vs->ecmascript_fragile)
			ecmascript_reset_state(top_view->vs);
		if (!top_view->vs->ecmascript) break;
		newjsframe = JS_GetGlobalObject(top_view->vs->ecmascript->backend_data);

		/* Keep this unrolled this way. Will have to check document.domain
		 * JS property. */
		/* Note that this check is perhaps overparanoid. If top windows
		 * is alien but some other child window is not, we should still
		 * let the script walk thru. That'd mean moving the check to
		 * other individual properties in this switch. */
		if (compare_uri(vs->uri, top_view->vs->uri, URI_HOST))
			set_prop_object(&prop, newjsframe);
		else
			/****X*X*X*** SECURITY VIOLATION! RED ALERT, SHIELDS UP! ***X*X*X****\
			|* (Pasky was apparently looking at the Links2 JS code   .  ___ ^.^ *|
			\* for too long.)                                        `.(,_,)\o/ */
			set_prop_undef(&prop);
		break;
	}
	default:
		INTERNAL("Invalid ID %d in window_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

convert:
	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

static void location_goto(struct document_view *doc_view, unsigned char *url);

/* @window_class.setProperty */
static JSBool
window_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct view_state *vs;
	union jsval_union v;

	vs = JS_GetPrivate(ctx, obj); /* from @window_class */

	if (JSVAL_IS_STRING(id)) {
		jsval_to_value(ctx, &id, JSTYPE_STRING, &v);
		if (!strcmp(v.string, "location")) {
			struct document_view *doc_view = vs->doc_view;

			jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
			location_goto(doc_view, v.string);
			/* Do NOT touch our .location property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
		return JS_TRUE;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	default:
		INTERNAL("Invalid ID %d in window_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
}

static JSBool window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool window_open(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec window_funcs[] = {
	{ "alert",	window_alert,		1 },
	{ "open",	window_open,		3 },
	{ NULL }
};

/* @window_funcs{"alert"} */
static JSBool
window_alert(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	struct view_state *vs;
	union jsval_union v;
	struct jsval_property prop;

	vs = JS_GetPrivate(ctx, obj); /* from @window_class */

	set_prop_undef(&prop);

	if (argc != 1)
		return JS_TRUE;

	jsval_to_value(ctx, &argv[0], JSTYPE_STRING, &v);
	if (!v.string || !*v.string)
		return JS_TRUE;

	info_box(vs->doc_view->session->tab->term, MSGBOX_FREE_TEXT,
		N_("JavaScript Alert"), ALIGN_CENTER, stracpy(v.string));

	value_to_jsval(ctx, rval, &prop);
	return JS_TRUE;
}

struct delayed_open {
	struct session *ses;
	struct uri *uri;
};

static void
delayed_open(void *data)
{
	struct delayed_open *deo = data;

	assert(deo);
	open_uri_in_new_tab(deo->ses, deo->uri, 0, 0);
	done_uri(deo->uri);
	mem_free(deo);
}

/* @window_funcs{"open"} */
static JSBool
window_open(JSContext *ctx, JSObject *obj, uintN argc,jsval *argv, jsval *rval)
{
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	union jsval_union v;
	unsigned char *url;
	struct uri *uri;
	static time_t ratelimit_start;
	static int ratelimit_count;
	struct jsval_property prop;

	vs = JS_GetPrivate(ctx, obj); /* from @window_class */
	doc_view = vs->doc_view;
	ses = doc_view->session;

	set_prop_undef(&prop);

	if (get_opt_bool("ecmascript.block_window_opening")) {
#ifdef CONFIG_LEDS
		ses->status.popup_led->value = 'P';
#endif
		return JS_TRUE;
	}

	if (argc < 1) return JS_TRUE;

	/* Ratelimit window opening. Recursive window.open() is very nice.
	 * We permit at most 20 tabs in 2 seconds. The ratelimiter is very
	 * rough but shall suffice against the usual cases. */

	if (!ratelimit_start || time(NULL) - ratelimit_start > 2) {
		ratelimit_start = time(NULL);
		ratelimit_count = 0;
	} else {
		ratelimit_count++;
		if (ratelimit_count > 20)
			return JS_TRUE;
	}

	jsval_to_value(ctx, &argv[0], JSTYPE_STRING, &v);
	url = v.string;
	assert(url);
	/* TODO: Support for window naming and perhaps some window features? */

	url = join_urls(doc_view->document->uri,
	                trim_chars(url, ' ', 0));
	if (!url) return JS_TRUE;
	uri = get_uri(url, 0);
	mem_free(url);
	if (!uri) return JS_TRUE;

	if (!get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home")
	    && !get_cmd_opt_bool("anonymous")
	    && can_open_in_new(ses->tab->term)) {
		open_uri_in_new_window(ses, uri, ENV_ANY);
		set_prop_boolean(&prop, 1);
	} else {
		/* When opening a new tab, we might get rerendered, losing our
		 * context and triggerring a disaster, so postpone that. */
		struct delayed_open *deo = mem_calloc(1, sizeof(*deo));

		if (deo) {
			deo->ses = ses;
			deo->uri = get_uri_reference(uri);
			register_bottom_half((void (*)(void *)) delayed_open,
			                     deo);
			set_prop_boolean(&prop, 1);
		}
	}

	done_uri(uri);

	value_to_jsval(ctx, rval, &prop);
	return JS_TRUE;
}


/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static JSBool input_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool input_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @input_class object must have a @form_class parent.  */
static const JSClass input_class = {
	"input", /* here, we unleash ourselves */
	JSCLASS_HAS_PRIVATE,	/* struct form_state * */
	JS_PropertyStub, JS_PropertyStub,
	input_get_property, input_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum input_prop {
	JSP_INPUT_ACCESSKEY,
	JSP_INPUT_ALT,
	JSP_INPUT_CHECKED,
	JSP_INPUT_DEFAULT_CHECKED,
	JSP_INPUT_DEFAULT_VALUE,
	JSP_INPUT_DISABLED,
	JSP_INPUT_FORM,
	JSP_INPUT_MAX_LENGTH,
	JSP_INPUT_NAME,
	JSP_INPUT_READONLY,
	JSP_INPUT_SIZE,
	JSP_INPUT_SRC,
	JSP_INPUT_TABINDEX,
	JSP_INPUT_TYPE,
	JSP_INPUT_VALUE
};

/* XXX: Some of those are marked readonly just because we can't change them
 * safely now. Changing default* values would affect all open instances of the
 * document, leading to a potential security risk. Changing size and type would
 * require re-rendering the document (TODO), tabindex would require renumbering
 * of all links and whatnot. --pasky */
static const JSPropertySpec input_props[] = {
	{ "accessKey",	JSP_INPUT_ACCESSKEY,	JSPROP_ENUMERATE },
	{ "alt",	JSP_INPUT_ALT,		JSPROP_ENUMERATE },
	{ "checked",	JSP_INPUT_CHECKED,	JSPROP_ENUMERATE },
	{ "defaultChecked",JSP_INPUT_DEFAULT_CHECKED,JSPROP_ENUMERATE },
	{ "defaultValue",JSP_INPUT_DEFAULT_VALUE,JSPROP_ENUMERATE },
	{ "disabled",	JSP_INPUT_DISABLED,	JSPROP_ENUMERATE },
	{ "form",	JSP_INPUT_FORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "maxLength",	JSP_INPUT_MAX_LENGTH,	JSPROP_ENUMERATE },
	{ "name",	JSP_INPUT_NAME,		JSPROP_ENUMERATE },
	{ "readonly",	JSP_INPUT_READONLY,	JSPROP_ENUMERATE },
	{ "size",	JSP_INPUT_SIZE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "src",	JSP_INPUT_SRC,		JSPROP_ENUMERATE },
	{ "tabindex",	JSP_INPUT_TABINDEX,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "type",	JSP_INPUT_TYPE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "value",	JSP_INPUT_VALUE,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool input_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool input_click(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool input_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool input_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec input_funcs[] = {
	{ "blur",	input_blur,	0 },
	{ "click",	input_click,	0 },
	{ "focus",	input_focus,	0 },
	{ "select",	input_select,	0 },
	{ NULL }
};

/* @input_class.getProperty */
static JSBool
input_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct jsval_property prop;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = JS_GetPrivate(ctx, obj); /* from @input_class */
	fc = find_form_control(document, fs);

	set_prop_undef(&prop);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
	{
		struct string keystr;

		if (!link) {
			set_prop_undef(&prop);
			break;
		}
		init_string(&keystr);
		make_keystroke(&keystr, link->accesskey, 0, 0);
		set_prop_string(&prop, keystr.source);
		done_string(&keystr);
		break;
	}
	case JSP_INPUT_ALT:
		set_prop_string(&prop, fc->alt);
		break;
	case JSP_INPUT_CHECKED:
		set_prop_boolean(&prop, fs->state);
		break;
	case JSP_INPUT_DEFAULT_CHECKED:
		set_prop_boolean(&prop, fc->default_state);
		break;
	case JSP_INPUT_DEFAULT_VALUE:
		set_prop_string(&prop, fc->default_value);
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		set_prop_boolean(&prop, fc->mode == FORM_MODE_DISABLED);
		break;
	case JSP_INPUT_FORM:
		set_prop_object(&prop, parent_form);
		break;
	case JSP_INPUT_MAX_LENGTH:
		set_prop_int(&prop, fc->maxlength);
		break;
	case JSP_INPUT_NAME:
		set_prop_string(&prop, fc->name);
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		set_prop_boolean(&prop, fc->mode == FORM_MODE_READONLY);
		break;
	case JSP_INPUT_SIZE:
		set_prop_int(&prop, fc->size);
		break;
	case JSP_INPUT_SRC:
		if (link && link->where_img)
			set_prop_string(&prop, link->where_img);
		else
			set_prop_undef(&prop);
		break;
	case JSP_INPUT_TABINDEX:
		if (link)
			/* FIXME: This is WRONG. --pasky */
			set_prop_int(&prop, link->number);
		else
			set_prop_undef(&prop);
		break;
	case JSP_INPUT_TYPE:
		switch (fc->type) {
		case FC_TEXT: set_prop_string(&prop, "text"); break;
		case FC_PASSWORD: set_prop_string(&prop, "password"); break;
		case FC_FILE: set_prop_string(&prop, "file"); break;
		case FC_CHECKBOX: set_prop_string(&prop, "checkbox"); break;
		case FC_RADIO: set_prop_string(&prop, "radio"); break;
		case FC_SUBMIT: set_prop_string(&prop, "submit"); break;
		case FC_IMAGE: set_prop_string(&prop, "image"); break;
		case FC_RESET: set_prop_string(&prop, "reset"); break;
		case FC_BUTTON: set_prop_string(&prop, "button"); break;
		case FC_HIDDEN: set_prop_string(&prop, "hidden"); break;
		default: INTERNAL("input_get_property() upon a non-input item."); break;
		}
		break;
	case JSP_INPUT_VALUE:
		set_prop_string(&prop, fs->value);
		break;

	default:
		INTERNAL("Invalid ID %d in input_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* @input_class.setProperty */
static JSBool
input_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct form_control *fc;
	int linknum;
	struct link *link = NULL;
	union jsval_union v;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = JS_GetPrivate(ctx, obj); /* from @input_class */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		if (link) link->accesskey = read_key(v.string);
		break;
	case JSP_INPUT_ALT:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		mem_free_set(&fc->alt, stracpy(v.string));
		break;
	case JSP_INPUT_CHECKED:
		if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO)
			break;
		jsval_to_value(ctx, vp, JSTYPE_BOOLEAN, &v);
		fs->state = v.boolean;
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		jsval_to_value(ctx, vp, JSTYPE_BOOLEAN, &v);
		fc->mode = (v.boolean ? FORM_MODE_DISABLED
		                      : fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_MAX_LENGTH:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		fc->maxlength = atol(v.string);
		break;
	case JSP_INPUT_NAME:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		mem_free_set(&fc->name, stracpy(v.string));
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		jsval_to_value(ctx, vp, JSTYPE_BOOLEAN, &v);
		fc->mode = (v.boolean ? FORM_MODE_READONLY
		                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_SRC:
		if (link) {
			jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
			mem_free_set(&link->where_img, stracpy(v.string));
		}
		break;
	case JSP_INPUT_VALUE:
		if (fc->type == FC_FILE)
			break; /* A huge security risk otherwise. */
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		mem_free_set(&fs->value, stracpy(v.string));
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
		break;

	default:
		INTERNAL("Invalid ID %d in input_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
}

/* @input_funcs{"blur"} */
static JSBool
input_blur(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	return JS_TRUE;
}

/* @input_funcs{"click"} */
static JSBool
input_click(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct form_control *fc;
	int linknum;
	struct jsval_property prop;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = JS_GetPrivate(ctx, obj); /* from @input_class */

	set_prop_boolean(&prop, 0);

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return JS_TRUE;

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);
	else
		print_screen_status(ses);

	value_to_jsval(ctx, rval, &prop);
	return JS_TRUE;
}

/* @input_funcs{"focus"} */
static JSBool
input_focus(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct form_control *fc;
	int linknum;
	struct jsval_property prop;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = JS_GetPrivate(ctx, obj); /* from @input_class */

	set_prop_boolean(&prop, 0);

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return JS_TRUE;

	jump_to_link_number(ses, doc_view, linknum);

	value_to_jsval(ctx, rval, &prop);
	return JS_TRUE;
}

/* @input_funcs{"select"} */
static JSBool
input_select(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	return JS_TRUE;
}

static JSObject *
get_input_object(JSContext *ctx, JSObject *jsform, struct form_state *fs)
{
	if (!fs->ecmascript_obj) {
		/* jsform ('form') is input's parent */
		/* FIXME: That is NOT correct since the real containing element
		 * should be its parent, but gimme DOM first. --pasky */
		JSObject *jsinput = JS_NewObject(ctx, (JSClass *) &input_class, NULL, jsform);

		JS_DefineProperties(ctx, jsinput, (JSPropertySpec *) input_props);
		JS_DefineFunctions(ctx, jsinput, (JSFunctionSpec *) input_funcs);
		JS_SetPrivate(ctx, jsinput, fs); /* to @input_class */
		fs->ecmascript_obj = jsinput;
	}
	return fs->ecmascript_obj;
}


static JSObject *
get_form_control_object(JSContext *ctx, JSObject *jsform, enum form_type type, struct form_state *fs)
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
			return get_input_object(ctx, jsform, fs);

		case FC_TEXTAREA:
		case FC_SELECT:
			/* TODO */
			return NULL;

		default:
			INTERNAL("Weird fc->type %d", type);
			return NULL;
	}
}



static JSBool form_elements_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @form_elements_class object must have a @form_class parent.  */
static const JSClass form_elements_class = {
	"elements",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	form_elements_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool form_elements_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool form_elements_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec form_elements_funcs[] = {
	{ "item",		form_elements_item,		1 },
	{ "namedItem",		form_elements_namedItem,	1 },
	{ NULL }
};

/* INTs from 0 up are equivalent to item(INT), so we have to stuff length out
 * of the way. */
enum form_elements_prop { JSP_FORM_ELEMENTS_LENGTH = -1 };
static const JSPropertySpec form_elements_props[] = {
	{ "length",	JSP_FORM_ELEMENTS_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

/* @form_elements_class.getProperty */
static JSBool
form_elements_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct jsval_property prop;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = JS_GetPrivate(ctx, parent_form); /* from @form_class */
	form = find_form_by_form_view(document, form_view);

	set_prop_undef(&prop);

	if (JSVAL_IS_STRING(id)) {
		form_elements_namedItem(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ELEMENTS_LENGTH:
	{
		struct form_control *fc;
		int counter = 0;

		foreach (fc, form->items)
			counter++;

		set_prop_int(&prop, counter);
		break;
	}
	default:
		/* Array index. */
		form_elements_item(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	}

	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* @form_elements_funcs{"item"} */
static JSBool
form_elements_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct form_control *fc;
	union jsval_union v;
	int counter = -1;
	int index;
	struct jsval_property prop;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = JS_GetPrivate(ctx, parent_form); /* from @form_class */
	form = find_form_by_form_view(document, form_view);

	set_prop_undef(&prop);

	if (argc != 1)
		return JS_TRUE;

	jsval_to_value(ctx, &argv[0], JSTYPE_STRING, &v);
	index = atol(v.string);

	foreach (fc, form->items) {
		counter++;
		if (counter == index) {
			JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, find_form_state(doc_view, fc));

			if (fcobj) {
				set_prop_object(&prop, fcobj);
			} else {
				set_prop_undef(&prop);
			}

			value_to_jsval(ctx, rval, &prop);
			return JS_TRUE;
		}
	}

	return JS_TRUE;
}

/* @form_elements_funcs{"namedItem"} */
static JSBool
form_elements_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_form;	/* instance of @form_class */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_view *form_view;
	struct form *form;
	struct form_control *fc;
	union jsval_union v;
	struct jsval_property prop;

	parent_form = JS_GetParent(ctx, obj);
	parent_doc = JS_GetParent(ctx, parent_form);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = JS_GetPrivate(ctx, parent_form); /* from @form_class */
	form = find_form_by_form_view(document, form_view);

	set_prop_undef(&prop);

	if (argc != 1)
		return JS_TRUE;

	jsval_to_value(ctx, &argv[0], JSTYPE_STRING, &v);
	if (!v.string || !*v.string)
		return JS_TRUE;

	foreach (fc, form->items) {
		if (fc->name && !strcasecmp(v.string, fc->name)) {
			JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, find_form_state(doc_view, fc));

			if (fcobj) {
				set_prop_object(&prop, fcobj);
			} else {
				set_prop_undef(&prop);
			}

			value_to_jsval(ctx, rval, &prop);
			return JS_TRUE;
		}
	}

	return JS_TRUE;
}



static JSBool form_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool form_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @form_class object must have a @document_class parent.  */
static const JSClass form_class = {
	"form",
	JSCLASS_HAS_PRIVATE,	/* struct form_view * */
	JS_PropertyStub, JS_PropertyStub,
	form_get_property, form_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum form_prop {
	JSP_FORM_ACTION,
	JSP_FORM_ELEMENTS,
	JSP_FORM_ENCODING,
	JSP_FORM_LENGTH,
	JSP_FORM_METHOD,
	JSP_FORM_NAME,
	JSP_FORM_TARGET
};

static const JSPropertySpec form_props[] = {
	{ "action",	JSP_FORM_ACTION,	JSPROP_ENUMERATE },
	{ "elements",	JSP_FORM_ELEMENTS,	JSPROP_ENUMERATE },
	{ "encoding",	JSP_FORM_ENCODING,	JSPROP_ENUMERATE },
	{ "length",	JSP_FORM_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "method",	JSP_FORM_METHOD,	JSPROP_ENUMERATE },
	{ "name",	JSP_FORM_NAME,		JSPROP_ENUMERATE },
	{ "target",	JSP_FORM_TARGET,	JSPROP_ENUMERATE },
	{ NULL }
};

static JSBool form_reset(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool form_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec form_funcs[] = {
	{ "reset",	form_reset,	0 },
	{ "submit",	form_submit,	0 },
	{ NULL }
};

/* @form_class.getProperty */
static JSBool
form_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	/* DBG("doc %p %s\n", parent_doc, JS_GetStringBytes(JS_ValueToString(ctx, OBJECT_TO_JSVAL(parent_doc)))); */
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct jsval_property prop;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	fv = JS_GetPrivate(ctx, obj); /* from @form_class */
	form = find_form_by_form_view(doc_view->document, fv);

	set_prop_undef(&prop);

	assert(form);

	if (JSVAL_IS_STRING(id)) {
		struct form_control *fc;
		union jsval_union v;

		jsval_to_value(ctx, &id, JSTYPE_STRING, &v);
		foreach (fc, form->items) {
			JSObject *fcobj = NULL;

			if (!fc->name || strcasecmp(v.string, fc->name))
				continue;

			fcobj = get_form_control_object(ctx, obj, fc->type, find_form_state(doc_view, fc));
			if (fcobj) {
				set_prop_object(&prop, fcobj);
			} else {
				set_prop_undef(&prop);
			}
			goto convert;
		}
		return JS_TRUE;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		set_prop_string(&prop, form->action);
		break;

	case JSP_FORM_ELEMENTS:
	{
		/* jsform ('form') is form_elements' parent; who knows is that's correct */
		JSObject *jsform_elems = JS_NewObject(ctx, (JSClass *) &form_elements_class, NULL, obj);

		JS_DefineProperties(ctx, jsform_elems, (JSPropertySpec *) form_elements_props);
		JS_DefineFunctions(ctx, jsform_elems, (JSFunctionSpec *) form_elements_funcs);
		set_prop_object(&prop, jsform_elems);
		/* SM will cache this property value for us so we create this
		 * just once per form. */
	}
		break;

	case JSP_FORM_ENCODING:
		switch (form->method) {
		case FORM_METHOD_GET:
		case FORM_METHOD_POST:
			set_prop_string(&prop, "application/x-www-form-urlencoded");
			break;
		case FORM_METHOD_POST_MP:
			set_prop_string(&prop, "multipart/form-data");
			break;
		case FORM_METHOD_POST_TEXT_PLAIN:
			set_prop_string(&prop, "text/plain");
			break;
		}
		break;

	case JSP_FORM_LENGTH:
	{
		struct form_control *fc;
		int counter = 0;

		foreach (fc, form->items)
			counter++;
		set_prop_int(&prop, counter);
		break;
	}

	case JSP_FORM_METHOD:
		switch (form->method) {
		case FORM_METHOD_GET:
			set_prop_string(&prop, "GET");
			break;

		case FORM_METHOD_POST:
		case FORM_METHOD_POST_MP:
		case FORM_METHOD_POST_TEXT_PLAIN:
			set_prop_string(&prop, "POST");
			break;
		}
		break;

	case JSP_FORM_NAME:
		set_prop_string(&prop, form->name);
		break;

	case JSP_FORM_TARGET:
		set_prop_string(&prop, form->target);
		break;

	default:
		INTERNAL("Invalid ID %d in form_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

convert:
	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* @form_class.setProperty */
static JSBool
form_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	union jsval_union v;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	fv = JS_GetPrivate(ctx, obj); /* from @form_class */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		mem_free_set(&form->action, stracpy(v.string));
		break;

	case JSP_FORM_ENCODING:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		if (!strcasecmp(v.string, "application/x-www-form-urlencoded")) {
			form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
			                                               : FORM_METHOD_POST;
		} else if (!strcasecmp(v.string, "multipart/form-data")) {
			form->method = FORM_METHOD_POST_MP;
		} else if (!strcasecmp(v.string, "text/plain")) {
			form->method = FORM_METHOD_POST_TEXT_PLAIN;
		}
		break;

	case JSP_FORM_METHOD:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		if (!strcasecmp(v.string, "GET")) {
			form->method = FORM_METHOD_GET;
		} else if (!strcasecmp(v.string, "POST")) {
			form->method = FORM_METHOD_POST;
		}
		break;

	case JSP_FORM_NAME:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		mem_free_set(&form->name, stracpy(v.string));
		break;

	case JSP_FORM_TARGET:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		mem_free_set(&form->target, stracpy(v.string));
		break;

	default:
		INTERNAL("Invalid ID %d in form_set_property().", JSVAL_TO_INT(id));
		break;
	}

	return JS_TRUE;
}

/* @form_funcs{"reset"} */
static JSBool
form_reset(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct form_view *fv;
	struct form *form;
	struct jsval_property prop;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	fv = JS_GetPrivate(ctx, obj); /* from @form_class */
	form = find_form_by_form_view(doc_view->document, fv);

	set_prop_boolean(&prop, 0);

	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	value_to_jsval(ctx, rval, &prop);

	return JS_TRUE;
}

/* @form_funcs{"submit"} */
static JSBool
form_submit(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct session *ses;
	struct form_view *fv;
	struct form *form;
	struct jsval_property prop;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	ses = doc_view->session;
	fv = JS_GetPrivate(ctx, obj); /* from @form_class */
	form = find_form_by_form_view(doc_view->document, fv);

	set_prop_boolean(&prop, 0);

	assert(form);
	submit_given_form(ses, doc_view, form);

	value_to_jsval(ctx, rval, &prop);

	return JS_TRUE;
}

static JSObject *
get_form_object(JSContext *ctx, JSObject *jsdoc, struct form_view *fv)
{
	if (!fv->ecmascript_obj) {
		/* jsdoc ('document') is fv's parent */
		/* FIXME: That is NOT correct since the real containing element
		 * should be its parent, but gimme DOM first. --pasky */
		JSObject *jsform = JS_NewObject(ctx, (JSClass *) &form_class, NULL, jsdoc);

		JS_DefineProperties(ctx, jsform, (JSPropertySpec *) form_props);
		JS_DefineFunctions(ctx, jsform, (JSFunctionSpec *) form_funcs);
		JS_SetPrivate(ctx, jsform, fv); /* to @form_class */
		fv->ecmascript_obj = jsform;
	}
	return fv->ecmascript_obj;
}


static JSBool forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @forms_class object must have a @document_class parent.  */
static const JSClass forms_class = {
	"forms",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	forms_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool forms_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec forms_funcs[] = {
	{ "item",		forms_item,		1 },
	{ "namedItem",		forms_namedItem,	1 },
	{ NULL }
};

/* INTs from 0 up are equivalent to item(INT), so we have to stuff length out
 * of the way. */
enum forms_prop { JSP_FORMS_LENGTH = -1 };
static const JSPropertySpec forms_props[] = {
	{ "length",	JSP_FORMS_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

/* @forms_class.getProperty */
static JSBool
forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct jsval_property prop;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;

	set_prop_undef(&prop);

	if (JSVAL_IS_STRING(id)) {
		forms_namedItem(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORMS_LENGTH:
	{
		struct form *form;
		int counter = 0;

		foreach (form, document->forms)
			counter++;

		set_prop_int(&prop, counter);
		break;
	}
	default:
		/* Array index. */
		forms_item(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	}

	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* forms_funcs{"item"} */
static JSBool
forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct form_view *fv;
	union jsval_union v;
	int counter = -1;
	int index;
	struct jsval_property prop;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */

	set_prop_undef(&prop);

	if (argc != 1)
		return JS_TRUE;

	jsval_to_value(ctx, &argv[0], JSTYPE_STRING, &v);
	index = atol(v.string);

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			set_prop_object(&prop, get_form_object(ctx, parent_doc, fv));

			value_to_jsval(ctx, rval, &prop);
			return JS_TRUE;
		}
	}

	return JS_TRUE;
}

/* @forms_funcs{"namedItem"} */
static JSBool
forms_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form *form;
	union jsval_union v;
	struct jsval_property prop;

	parent_doc = JS_GetParent(ctx, obj);
	parent_win = JS_GetParent(ctx, parent_doc);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;

	set_prop_undef(&prop);

	if (argc != 1)
		return JS_TRUE;

	jsval_to_value(ctx, &argv[0], JSTYPE_STRING, &v);
	if (!v.string || !*v.string)
		return JS_TRUE;

	foreach (form, document->forms) {
		if (form->name && !strcasecmp(v.string, form->name)) {
			set_prop_object(&prop, get_form_object(ctx, parent_doc,
					find_form_view(doc_view, form)));

			value_to_jsval(ctx, rval, &prop);
			return JS_TRUE;
		}
	}

	return JS_TRUE;
}


static JSBool document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @document_class object must have a @window_class parent.  */
static const JSClass document_class = {
	"document",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	document_get_property, document_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum document_prop { JSP_DOC_REF, JSP_DOC_TITLE, JSP_DOC_URL };
/* "cookie" is special; it isn't a regular property but we channel it to the
 * cookie-module. XXX: Would it work if "cookie" was defined in this array? */
static const JSPropertySpec document_props[] = {
	{ "location",	JSP_DOC_URL,	JSPROP_ENUMERATE },
	{ "referrer",	JSP_DOC_REF,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "title",	JSP_DOC_TITLE,	JSPROP_ENUMERATE }, /* TODO: Charset? */
	{ "url",	JSP_DOC_URL,	JSPROP_ENUMERATE },
	{ NULL }
};

/* @document_class.getProperty */
static JSBool
document_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct jsval_property prop;

	parent_win = JS_GetParent(ctx, obj);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;

	set_prop_undef(&prop);

	if (JSVAL_IS_STRING(id)) {
		struct form *form;
		union jsval_union v;

		jsval_to_value(ctx, &id, JSTYPE_STRING, &v);
#ifdef CONFIG_COOKIES
		if (!strcmp(v.string, "cookie")) {
			struct string *cookies = send_cookies(vs->uri);

			if (cookies) {
				static unsigned char cookiestr[1024];

				strncpy(cookiestr, cookies->source, 1024);
				done_string(cookies);
				set_prop_string(&prop, cookiestr);
				goto convert;
			}
		}
#endif
		foreach (form, document->forms) {
			if (!form->name || strcasecmp(v.string, form->name))
				continue;

			set_prop_object(&prop, get_form_object(ctx, obj, find_form_view(doc_view, form)));
			goto convert;
		}
		return JS_TRUE;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_REF:
		switch (get_opt_int("protocol.http.referer.policy")) {
		case REFERER_NONE:
			/* oh well */
			break;

		case REFERER_FAKE:
			set_prop_string(&prop, get_opt_str("protocol.http.referer.fake"));
			break;

		case REFERER_TRUE:
			/* XXX: Encode as in add_url_to_httset_prop_string(&prop, ) ? --pasky */
			if (ses->referrer) {
				set_prop_astring(&prop, get_uri_string(ses->referrer, URI_HTTP_REFERRER));
			}
			break;

		case REFERER_SAME_URL:
			set_prop_astring(&prop, get_uri_string(document->uri, URI_HTTP_REFERRER));
			break;
		}
		break;
	case JSP_DOC_TITLE:
		set_prop_string(&prop, document->title);
		break;
	case JSP_DOC_URL:
		set_prop_astring(&prop, get_uri_string(document->uri, URI_ORIGINAL));
		break;
	default:
		INTERNAL("Invalid ID %d in document_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

convert:
	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* @document_class.setProperty */
static JSBool
document_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	union jsval_union v;

	parent_win = JS_GetParent(ctx, obj);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	document = doc_view->document;

	if (JSVAL_IS_STRING(id)) {
		jsval_to_value(ctx, &id, JSTYPE_STRING, &v);
#ifdef CONFIG_COOKIES
		if (!strcmp(v.string, "cookie")) {
			jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
			set_cookie(vs->uri, v.string);
			/* Do NOT touch our .cookie property, evil
			 * SpiderMonkey!! */
			return JS_FALSE;
		}
#endif
		return JS_TRUE;
	} else if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_DOC_TITLE:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		if (document->title) mem_free(document->title);
		document->title = stracpy(v.string);
		break;
	case JSP_DOC_URL:
		/* According to the specs this should be readonly but some
		 * broken sites still assign to it (i.e.
		 * http://www.e-handelsfonden.dk/validering.asp?URL=www.polyteknisk.dk).
		 * So emulate window.location. */
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		location_goto(doc_view, v.string);
		break;
	}

	return JS_TRUE;
}

static JSBool document_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec document_funcs[] = {
	{ "write",		document_write,		1 },
	{ NULL }
};

/* @document_funcs{"write"} */
static JSBool
document_write(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
#ifdef CONFIG_LEDS
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
#endif
	struct jsval_property prop;

	set_prop_boolean(&prop, 0);

	/* XXX: I don't know about you, but I have *ENOUGH* of those 'Undefined
	 * function' errors, I want to see just the useful ones. So just
	 * lighting a led and going away, no muss, no fuss. --pasky */
	/* TODO: Perhaps we can introduce ecmascript.error_report_unsupported
	 * -> "Show information about the document using some valid,
	 *  nevertheless unsupported methods/properties." --pasky too */

#ifdef CONFIG_LEDS
	interpreter->vs->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	value_to_jsval(ctx, rval, &prop);

	return JS_TRUE;
}



static JSBool location_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool location_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @location_class object must have a @window_class parent.  */
static const JSClass location_class = {
	"location",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	location_get_property, location_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum location_prop { JSP_LOC_HREF };
static const JSPropertySpec location_props[] = {
	{ "href",	JSP_LOC_HREF,	JSPROP_ENUMERATE },
	{ NULL }
};

/* @location_class.getProperty */
static JSBool
location_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct jsval_property prop;

	parent_win = JS_GetParent(ctx, obj);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */

	set_prop_undef(&prop);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
		set_prop_astring(&prop, get_uri_string(vs->uri, URI_ORIGINAL));
		break;
	default:
		INTERNAL("Invalid ID %d in location_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* @location_class.setProperty */
static JSBool
location_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	union jsval_union v;

	parent_win = JS_GetParent(ctx, obj);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_LOC_HREF:
		jsval_to_value(ctx, vp, JSTYPE_STRING, &v);
		location_goto(doc_view, v.string);
		break;
	}

	return JS_TRUE;
}

static JSBool location_toString(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static const JSFunctionSpec location_funcs[] = {
	{ "toString",		location_toString,	0 },
	{ "toLocaleString",	location_toString,	0 },
	{ NULL }
};

/* @location_funcs{"toString"}, @location_funcs{"toLocaleString"} */
static JSBool
location_toString(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_GetProperty(ctx, obj, "href", rval);
}

struct delayed_goto {
	/* It might look more convenient to pass doc_view around but it could
	 * disappear during wild dances inside of frames or so. */
	struct view_state *vs;
	struct uri *uri;
};

static void
delayed_goto(void *data)
{
	struct delayed_goto *deg = data;

	assert(deg);
	if (deg->vs->doc_view) {
		goto_uri_frame(deg->vs->doc_view->session, deg->uri,
		               deg->vs->doc_view->name,
			       CACHE_MODE_NORMAL);
	}
	done_uri(deg->uri);
	mem_free(deg);
}

static void
location_goto(struct document_view *doc_view, unsigned char *url)
{
	unsigned char *new_abs_url;
	struct uri *new_uri;
	struct delayed_goto *deg;

	new_abs_url = join_urls(doc_view->document->uri,
	                        trim_chars(url, ' ', 0));
	if (!new_abs_url)
		return;
	new_uri = get_uri(new_abs_url, 0);
	mem_free(new_abs_url);
	if (!new_uri)
		return;
	deg = mem_calloc(1, sizeof(*deg));
	if (!deg) {
		done_uri(new_uri);
		return;
	}
	assert(doc_view->vs);
	deg->vs = doc_view->vs;
	deg->uri = new_uri;
	/* It does not seem to be very safe inside of frames to
	 * call goto_uri() right away. */
	register_bottom_half((void (*)(void *)) delayed_goto, deg);
}


static JSBool unibar_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @menubar_class object must have a @window_class parent.  */
static const JSClass menubar_class = {
	"menubar",
	JSCLASS_HAS_PRIVATE,	/* const char * "t" */
	JS_PropertyStub, JS_PropertyStub,
	unibar_get_property, unibar_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};
/* Each @statusbar_class object must have a @window_class parent.  */
static const JSClass statusbar_class = {
	"statusbar",
	JSCLASS_HAS_PRIVATE,	/* const char * "s" */
	JS_PropertyStub, JS_PropertyStub,
	unibar_get_property, unibar_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum unibar_prop { JSP_UNIBAR_VISIBLE };
static const JSPropertySpec unibar_props[] = {
	{ "visible",	JSP_UNIBAR_VISIBLE,	JSPROP_ENUMERATE },
	{ NULL }
};


/* @menubar_class.getProperty, @statusbar_class.getProperty */
static JSBool
unibar_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct session_status *status;
	unsigned char *bar;
	struct jsval_property prop;

	parent_win = JS_GetParent(ctx, obj);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	status = &doc_view->session->status;
	bar = JS_GetPrivate(ctx, obj); /* from @menubar_class or @statusbar_class */

	set_prop_undef(&prop);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
#define unibar_fetch(bar) \
	set_prop_boolean(&prop, status->force_show_##bar##_bar >= 0 \
	          ? status->force_show_##bar##_bar \
	          : status->show_##bar##_bar)
		switch (*bar) {
		case 's':
			unibar_fetch(status);
			break;
		case 't':
			unibar_fetch(title);
			break;
		default:
			set_prop_boolean(&prop, 0);
			break;
		}
#undef unibar_fetch
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}

/* @menubar_class.setProperty, @statusbar_class.setProperty */
static JSBool
unibar_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct session_status *status;
	unsigned char *bar;
	union jsval_union v;

	parent_win = JS_GetParent(ctx, obj);
	vs = JS_GetPrivate(ctx, parent_win); /* from @window_class */
	doc_view = vs->doc_view;
	status = &doc_view->session->status;
	bar = JS_GetPrivate(ctx, obj); /* from @menubar_class or @statusbar_class */

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_UNIBAR_VISIBLE:
		jsval_to_value(ctx, vp, JSTYPE_BOOLEAN, &v);
#define unibar_set(bar) \
	status->force_show_##bar##_bar = v.boolean;
		switch (*bar) {
		case 's':
			unibar_set(status);
			break;
		case 't':
			unibar_set(title);
			break;
		default:
			v.boolean = 0;
			break;
		}
		register_bottom_half((void (*)(void*)) update_status, NULL);
#undef unibar_set
		break;
	default:
		INTERNAL("Invalid ID %d in unibar_set_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	return JS_TRUE;
}


static JSBool navigator_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

static const JSClass navigator_class = {
	"navigator",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	navigator_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

enum navigator_prop {
	JSP_NAVIGATOR_APP_CODENAME,
	JSP_NAVIGATOR_APP_NAME,
	JSP_NAVIGATOR_APP_VERSION,
	JSP_NAVIGATOR_LANGUAGE,
	/* JSP_NAVIGATOR_MIME_TYPES, */
	JSP_NAVIGATOR_PLATFORM,
	/* JSP_NAVIGATOR_PLUGINS, */
	JSP_NAVIGATOR_USER_AGENT,
};
static const JSPropertySpec navigator_props[] = {
	{ "appCodeName",	JSP_NAVIGATOR_APP_CODENAME,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "appName",		JSP_NAVIGATOR_APP_NAME,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "appVersion",		JSP_NAVIGATOR_APP_VERSION,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "language",		JSP_NAVIGATOR_LANGUAGE,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "platform",		JSP_NAVIGATOR_PLATFORM,		JSPROP_ENUMERATE | JSPROP_READONLY },
	{ "userAgent",		JSP_NAVIGATOR_USER_AGENT,	JSPROP_ENUMERATE | JSPROP_READONLY },
	{ NULL }
};


/* @navigator_class.getProperty */
static JSBool
navigator_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	struct jsval_property prop;

	set_prop_undef(&prop);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_NAVIGATOR_APP_CODENAME:
		set_prop_string(&prop, "Mozilla"); /* More like a constant nowadays. */
		break;
	case JSP_NAVIGATOR_APP_NAME:
		/* This evil hack makes the compatibility checking .indexOf()
		 * code find what it's looking for. */
		set_prop_string(&prop, "ELinks (roughly compatible with Netscape Navigator, Mozilla and Microsoft Internet Explorer)");
		break;
	case JSP_NAVIGATOR_APP_VERSION:
		set_prop_string(&prop, VERSION);
		break;
	case JSP_NAVIGATOR_LANGUAGE:
#ifdef ENABLE_NLS
		if (get_opt_bool("protocol.http.accept_ui_language"))
			set_prop_string(&prop, language_to_iso639(current_language));
		else
#endif
			set_prop_undef(&prop);
		break;
	case JSP_NAVIGATOR_PLATFORM:
		set_prop_string(&prop, system_name);
		break;
	case JSP_NAVIGATOR_USER_AGENT:
	{
		/* FIXME: Code duplication. */
		unsigned char *optstr = get_opt_str("protocol.http.user_agent");

		if (*optstr && strcmp(optstr, " ")) {
			unsigned char *ustr, ts[64] = "";
			static unsigned char custr[256];

			if (!list_empty(terminals)) {
				unsigned int tslen = 0;
				struct terminal *term = terminals.prev;

				ulongcat(ts, &tslen, term->width, 3, 0);
				ts[tslen++] = 'x';
				ulongcat(ts, &tslen, term->height, 3, 0);
			}
			ustr = subst_user_agent(optstr, VERSION_STRING, system_name, ts);

			if (ustr) {
				safe_strncpy(custr, ustr, 256);
				mem_free(ustr);
				set_prop_string(&prop, custr);
			} else{
				set_prop_undef(&prop);
			}
		}
	}
		break;
	default:
		INTERNAL("Invalid ID %d in navigator_get_property().", JSVAL_TO_INT(id));
		return JS_TRUE;
	}

	value_to_jsval(ctx, vp, &prop);
	return JS_TRUE;
}



/*** The ELinks interface */

static JSRuntime *jsrt;

static void
error_reporter(JSContext *ctx, const char *message, JSErrorReport *report)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	struct terminal *term;
	unsigned char *strict, *exception, *warning, *error;
	struct string msg;

	assert(interpreter && interpreter->vs && interpreter->vs->doc_view
	       && interpreter->vs->doc_view->session
	       && interpreter->vs->doc_view->session->tab);
	if_assert_failed goto reported;

	term = interpreter->vs->doc_view->session->tab->term;

#ifdef CONFIG_LEDS
	interpreter->vs->doc_view->session->status.ecmascript_led->value = 'J';
#endif

	if (!get_opt_bool("ecmascript.error_reporting")
	    || !init_string(&msg))
		goto reported;

	strict	  = JSREPORT_IS_STRICT(report->flags) ? " strict" : "";
	exception = JSREPORT_IS_EXCEPTION(report->flags) ? " exception" : "";
	warning   = JSREPORT_IS_WARNING(report->flags) ? " warning" : "";
	error	  = !report->flags ? " error" : "";

	add_format_to_string(&msg, _("A script embedded in the current "
			"document raised the following%s%s%s%s", term),
			strict, exception, warning, error);

	add_to_string(&msg, ":\n\n");
	add_to_string(&msg, message);

	if (report->linebuf && report->tokenptr) {
		int pos = report->tokenptr - report->linebuf;

		add_format_to_string(&msg, "\n\n%s\n.%*s^%*s.",
			       report->linebuf,
			       pos - 2, " ",
			       strlen(report->linebuf) - pos - 1, " ");
	}

	info_box(term, MSGBOX_FREE_TEXT, N_("JavaScript Error"), ALIGN_CENTER,
		 msg.source);

reported:
	/* Im clu'les. --pasky */
	JS_ClearPendingException(ctx);
}

static JSBool
safeguard(JSContext *ctx, JSScript *script)
{
	struct ecmascript_interpreter *interpreter = JS_GetContextPrivate(ctx);
	int max_exec_time = get_opt_int("ecmascript.max_exec_time");

	if (time(NULL) - interpreter->exec_start > max_exec_time) {
		struct terminal *term = interpreter->vs->doc_view->session->tab->term;

		/* A killer script! Alert! */
		info_box(term, MSGBOX_FREE_TEXT,
			 N_("JavaScript Emergency"), ALIGN_LEFT,
			 msg_text(term,
				  N_("A script embedded in the current document was running\n"
				  "for more than %d seconds. This probably means there is\n"
				  "a bug in the script and it could have halted the whole\n"
				  "ELinks, so the script execution was interrupted."),
				  max_exec_time));
		return JS_FALSE;
	}
	return JS_TRUE;
}

static void
setup_safeguard(struct ecmascript_interpreter *interpreter,
                JSContext *ctx)
{
	interpreter->exec_start = time(NULL);
	JS_SetBranchCallback(ctx, safeguard);
}


void
spidermonkey_init(void)
{
	jsrt = JS_NewRuntime(0x400000UL);
}

void
spidermonkey_done(void)
{
	JS_DestroyRuntime(jsrt);
	JS_ShutDown();
}


void *
spidermonkey_get_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;
	JSObject *window_obj, *document_obj, *forms_obj, *location_obj,
	         *statusbar_obj, *menubar_obj, *navigator_obj;

	assert(interpreter);

	ctx = JS_NewContext(jsrt, 8192 /* Stack allocation chunk size */);
	if (!ctx)
		return NULL;
	interpreter->backend_data = ctx;
	JS_SetContextPrivate(ctx, interpreter);
	/* TODO: Make JSOPTION_STRICT and JSOPTION_WERROR configurable. */
#ifndef JSOPTION_COMPILE_N_GO
#define JSOPTION_COMPILE_N_GO 0 /* Older SM versions don't have it. */
#endif
	/* XXX: JSOPTION_COMPILE_N_GO will go (will it?) when we implement
	 * some kind of bytecode cache. (If we will ever do that.) */
	JS_SetOptions(ctx, JSOPTION_VAROBJFIX | JSOPTION_COMPILE_N_GO);
	JS_SetErrorReporter(ctx, error_reporter);

	window_obj = JS_NewObject(ctx, (JSClass *) &window_class, NULL, NULL);
	if (!window_obj) {
		spidermonkey_put_interpreter(interpreter);
		return NULL;
	}
	JS_InitStandardClasses(ctx, window_obj);
	JS_DefineProperties(ctx, window_obj, (JSPropertySpec *) window_props);
	JS_DefineFunctions(ctx, window_obj, (JSFunctionSpec *) window_funcs);
	JS_SetPrivate(ctx, window_obj, interpreter->vs); /* to @window_class */

	document_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &document_class, NULL, 0,
				    (JSPropertySpec *) document_props,
				    (JSFunctionSpec *) document_funcs,
				    NULL, NULL);

	forms_obj = JS_InitClass(ctx, document_obj, NULL,
				    (JSClass *) &forms_class, NULL, 0,
				    (JSPropertySpec *) forms_props,
				    (JSFunctionSpec *) forms_funcs,
				    NULL, NULL);

	location_obj = JS_InitClass(ctx, window_obj, NULL,
				    (JSClass *) &location_class, NULL, 0,
				    (JSPropertySpec *) location_props,
				    (JSFunctionSpec *) location_funcs,
				    NULL, NULL);

	menubar_obj = JS_InitClass(ctx, window_obj, NULL,
				   (JSClass *) &menubar_class, NULL, 0,
				   (JSPropertySpec *) unibar_props, NULL,
				   NULL, NULL);
	JS_SetPrivate(ctx, menubar_obj, "t"); /* to @menubar_class */

	statusbar_obj = JS_InitClass(ctx, window_obj, NULL,
				     (JSClass *) &statusbar_class, NULL, 0,
				     (JSPropertySpec *) unibar_props, NULL,
				     NULL, NULL);
	JS_SetPrivate(ctx, statusbar_obj, "s"); /* to @statusbar_class */

	navigator_obj = JS_InitClass(ctx, window_obj, NULL,
				     (JSClass *) &navigator_class, NULL, 0,
				     (JSPropertySpec *) navigator_props, NULL,
				     NULL, NULL);

	return ctx;
}

void
spidermonkey_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	JSContext *ctx;

	assert(interpreter);
	ctx = interpreter->backend_data;
	JS_DestroyContext(ctx);
	interpreter->backend_data = NULL;
}


void
spidermonkey_eval(struct ecmascript_interpreter *interpreter,
                  struct string *code)
{
	JSContext *ctx;
	jsval rval;

	assert(interpreter);
	ctx = interpreter->backend_data;
	setup_safeguard(interpreter, ctx);
	JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
	                  code->source, code->length, "", 0, &rval);
}


unsigned char *
spidermonkey_eval_stringback(struct ecmascript_interpreter *interpreter,
			     struct string *code)
{
	JSContext *ctx;
	jsval rval;
	union jsval_union v;

	assert(interpreter);
	ctx = interpreter->backend_data;
	setup_safeguard(interpreter, ctx);
	if (JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
			      code->source, code->length, "", 0, &rval)
	    == JS_FALSE) {
		return NULL;
	}
	if (JSVAL_IS_VOID(rval)) {
		/* Undefined value. */
		return NULL;
	}

	jsval_to_value(ctx, &rval, JSTYPE_STRING, &v);
	if (!v.string) return NULL;

	return stracpy(v.string);
}


int
spidermonkey_eval_boolback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	JSContext *ctx;
	jsval rval;
	union jsval_union v;

	assert(interpreter);
	ctx = interpreter->backend_data;
	setup_safeguard(interpreter, ctx);
	if (JS_EvaluateScript(ctx, JS_GetGlobalObject(ctx),
			      code->source, code->length, "", 0, &rval)
	    == JS_FALSE) {
		return -1;
	}
	if (JSVAL_IS_VOID(rval)) {
		/* Undefined value. */
		return -1;
	}

	jsval_to_value(ctx, &rval, JSTYPE_BOOLEAN, &v);
	return v.boolean;
}
