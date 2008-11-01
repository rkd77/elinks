/* The SpiderMonkey window object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/spidermonkey/util.h"

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


static const JSClass form_class;	     /* defined below */


/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static JSBool input_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool input_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static void input_finalize(JSContext *ctx, JSObject *obj);

/* Each @input_class object must have a @form_class parent.  */
static const JSClass input_class = {
	"input", /* here, we unleash ourselves */
	JSCLASS_HAS_PRIVATE,	/* struct form_state *, or NULL if detached */
	JS_PropertyStub, JS_PropertyStub,
	input_get_property, input_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, input_finalize
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
	{ "selectedIndex",JSP_INPUT_SELECTED_INDEX,JSPROP_ENUMERATE },
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

static const spidermonkeyFunctionSpec input_funcs[] = {
	{ "blur",	input_blur,	0 },
	{ "click",	input_click,	0 },
	{ "focus",	input_focus,	0 },
	{ "select",	input_select,	0 },
	{ NULL }
};

static JSString *unicode_to_jsstring(JSContext *ctx, unicode_val_T u);
static unicode_val_T jsval_to_accesskey(JSContext *ctx, jsval *vp);


static struct form_state *
input_get_form_state(JSContext *ctx, JSObject *jsinput)
{
	struct form_state *fs = JS_GetInstancePrivate(ctx, jsinput,
						      (JSClass *) &input_class,
						      NULL);

	if (!fs) return NULL;	/* detached */

	assert(fs->ecmascript_obj == jsinput);
	if_assert_failed return NULL;

	return fs;
}

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

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &input_class, NULL))
		return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, obj);
	if (!fs) return JS_FALSE; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
	{
		JSString *keystr;

		if (!link) break;

		if (!link->accesskey) {
			*vp = JS_GetEmptyStringValue(ctx);
		} else {
			keystr = unicode_to_jsstring(ctx, link->accesskey);
			if (keystr)
				*vp = STRING_TO_JSVAL(keystr);
			else
				return JS_FALSE;
		}
		break;
	}
	case JSP_INPUT_ALT:
		string_to_jsval(ctx, vp, fc->alt);
		break;
	case JSP_INPUT_CHECKED:
		boolean_to_jsval(ctx, vp, fs->state);
		break;
	case JSP_INPUT_DEFAULT_CHECKED:
		boolean_to_jsval(ctx, vp, fc->default_state);
		break;
	case JSP_INPUT_DEFAULT_VALUE:
		/* FIXME (bug 805): convert from the charset of the document */
		string_to_jsval(ctx, vp, fc->default_value);
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		boolean_to_jsval(ctx, vp, fc->mode == FORM_MODE_DISABLED);
		break;
	case JSP_INPUT_FORM:
		object_to_jsval(ctx, vp, parent_form);
		break;
	case JSP_INPUT_MAX_LENGTH:
		int_to_jsval(ctx, vp, fc->maxlength);
		break;
	case JSP_INPUT_NAME:
		string_to_jsval(ctx, vp, fc->name);
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		boolean_to_jsval(ctx, vp, fc->mode == FORM_MODE_READONLY);
		break;
	case JSP_INPUT_SIZE:
		int_to_jsval(ctx, vp, fc->size);
		break;
	case JSP_INPUT_SRC:
		if (link && link->where_img)
			string_to_jsval(ctx, vp, link->where_img);
		break;
	case JSP_INPUT_TABINDEX:
		if (link)
			/* FIXME: This is WRONG. --pasky */
			int_to_jsval(ctx, vp, link->number);
		break;
	case JSP_INPUT_TYPE:
	{
		unsigned char *s = NULL;

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
		string_to_jsval(ctx, vp, s);
		break;
	}
	case JSP_INPUT_VALUE:
		string_to_jsval(ctx, vp, fs->value);
		break;

	case JSP_INPUT_SELECTED_INDEX:
		if (fc->type == FC_SELECT) int_to_jsval(ctx, vp, fs->state);
		break;
	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		break;
	}

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
	unicode_val_T accesskey;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &input_class, NULL))
		return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = input_get_form_state(ctx, obj);
	if (!fs) return JS_FALSE; /* detached */
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	switch (JSVAL_TO_INT(id)) {
	case JSP_INPUT_ACCESSKEY:
		accesskey = jsval_to_accesskey(ctx, vp);
		if (accesskey == UCS_NO_CHAR)
			return JS_FALSE;
		else if (link)
			link->accesskey = accesskey;
		break;
	case JSP_INPUT_ALT:
		mem_free_set(&fc->alt, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_INPUT_CHECKED:
		if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO)
			break;
		fs->state = jsval_to_boolean(ctx, vp);
		break;
	case JSP_INPUT_DISABLED:
		/* FIXME: <input readonly disabled> --pasky */
		fc->mode = (jsval_to_boolean(ctx, vp) ? FORM_MODE_DISABLED
		                      : fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_MAX_LENGTH:
		if (!JS_ValueToInt32(ctx, *vp, &fc->maxlength))
			return JS_FALSE;
		break;
	case JSP_INPUT_NAME:
		mem_free_set(&fc->name, stracpy(jsval_to_string(ctx, vp)));
		break;
	case JSP_INPUT_READONLY:
		/* FIXME: <input readonly disabled> --pasky */
		fc->mode = (jsval_to_boolean(ctx, vp) ? FORM_MODE_READONLY
		                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
		                                                       : FORM_MODE_NORMAL);
		break;
	case JSP_INPUT_SRC:
		if (link) {
			mem_free_set(&link->where_img, stracpy(jsval_to_string(ctx, vp)));
		}
		break;
	case JSP_INPUT_VALUE:
		if (fc->type == FC_FILE)
			break; /* A huge security risk otherwise. */
		mem_free_set(&fs->value, stracpy(jsval_to_string(ctx, vp)));
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
		break;
	case JSP_INPUT_SELECTED_INDEX:
		if (fc->type == FC_SELECT) {
			int item;

			if (!JS_ValueToInt32(ctx, *vp, &item))
				return JS_FALSE;

			if (item >= 0 && item < fc->nvalues) {
				fs->state = item;
				mem_free_set(&fs->value, stracpy(fc->values[item]));
			}
		}
		break;

	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
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

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &input_class, argv)) return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = input_get_form_state(ctx, obj);
	if (!fs) return JS_FALSE; /* detached */

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

	boolean_to_jsval(ctx, rval, 0);
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

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &input_class, argv)) return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = input_get_form_state(ctx, obj);
	if (!fs) return JS_FALSE; /* detached */

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0)
		return JS_TRUE;

	jump_to_link_number(ses, doc_view, linknum);

	boolean_to_jsval(ctx, rval, 0);
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
	JSObject *jsinput = fs->ecmascript_obj;

	if (jsinput) {
		/* This assumes JS_GetInstancePrivate cannot GC.  */
		assert(JS_GetInstancePrivate(ctx, jsinput,
					     (JSClass *) &input_class, NULL)
		       == fs);
		if_assert_failed return NULL;

		return jsinput;
	}

	/* jsform ('form') is input's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */
	jsinput = JS_NewObject(ctx, (JSClass *) &input_class, NULL, jsform);
	if (!jsinput)
		return NULL;
	JS_DefineProperties(ctx, jsinput, (JSPropertySpec *) input_props);
	spidermonkey_DefineFunctions(ctx, jsinput, input_funcs);

	if (!JS_SetPrivate(ctx, jsinput, fs)) /* to @input_class */
		return NULL;
	fs->ecmascript_obj = jsinput;
	return jsinput;
}

static void
input_finalize(JSContext *ctx, JSObject *jsinput)
{
	struct form_state *fs = JS_GetInstancePrivate(ctx, jsinput,
						      (JSClass *) &input_class,
						      NULL);

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
		/* This assumes JS_GetInstancePrivate and JS_SetPrivate
		 * cannot GC.  */

		/* If this assertion fails, it is not clear whether
		 * the private pointer of jsinput should be reset;
		 * crashes seem possible either way.  Resetting it is
		 * easiest.  */
		assert(JS_GetInstancePrivate(spidermonkey_empty_context,
					     jsinput,
					     (JSClass *) &input_class, NULL)
		       == fs);
		if_assert_failed {}

		JS_SetPrivate(spidermonkey_empty_context, jsinput, NULL);
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
		JS_SetPrivate(spidermonkey_empty_context, jsinput, fs);
	}
}


static JSObject *
get_form_control_object(JSContext *ctx, JSObject *jsform,
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
			return get_input_object(ctx, jsform, fs);

		case FC_TEXTAREA:
			/* TODO */
			return NULL;

		default:
			INTERNAL("Weird fc->type %d", type);
			return NULL;
	}
}


static struct form_view *form_get_form_view(JSContext *ctx, JSObject *jsform, jsval *argv);
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

static const spidermonkeyFunctionSpec form_elements_funcs[] = {
	{ "item",		form_elements_item,		1 },
	{ "namedItem",		form_elements_namedItem,	1 },
	{ NULL }
};

/* Tinyids of properties.  Use negative values to distinguish these
 * from array indexes (elements[INT] for INT>=0 is equivalent to
 * elements.item(INT)).  ECMAScript code should not use these directly
 * as in elements[-1]; future versions of ELinks may change the numbers.  */
enum form_elements_prop {
	JSP_FORM_ELEMENTS_LENGTH = -1,
};
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

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_elements_class, NULL))
		return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = form_get_form_view(ctx, parent_form, NULL);
	if (!form_view) return JS_FALSE; /* detached */
	form = find_form_by_form_view(document, form_view);

	if (JSVAL_IS_STRING(id)) {
		form_elements_namedItem(ctx, obj, 1, &id, vp);
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ELEMENTS_LENGTH:
		int_to_jsval(ctx, vp, list_size(&form->items));
		break;
	default:
		/* Array index. */
		form_elements_item(ctx, obj, 1, &id, vp);
		break;
	}

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
	int counter = -1;
	int index;

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_elements_class, argv)) return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = form_get_form_view(ctx, parent_form, NULL);
	if (!form_view) return JS_FALSE; /* detached */
	form = find_form_by_form_view(document, form_view);

	if (argc != 1)
		return JS_TRUE;

	if (!JS_ValueToInt32(ctx, argv[0], &index))
		return JS_FALSE;
	undef_to_jsval(ctx, rval);

	foreach (fc, form->items) {
		counter++;
		if (counter == index) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, fs);

				if (fcobj)
					object_to_jsval(ctx, rval, fcobj);
			}
			break;
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
	unsigned char *string;

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_elements_class, argv)) return JS_FALSE;
	parent_form = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_form, (JSClass *) &form_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_doc = JS_GetParent(ctx, parent_form);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;
	form_view = form_get_form_view(ctx, parent_form, NULL);
	if (!form_view) return JS_FALSE; /* detached */
	form = find_form_by_form_view(document, form_view);

	if (argc != 1)
		return JS_TRUE;

	string = jsval_to_string(ctx, &argv[0]);
	if (!*string)
		return JS_TRUE;

	undef_to_jsval(ctx, rval);

	foreach (fc, form->items) {
		if ((fc->id && !c_strcasecmp(string, fc->id))
		    || (fc->name && !c_strcasecmp(string, fc->name))) {
			struct form_state *fs = find_form_state(doc_view, fc);

			if (fs) {
				JSObject *fcobj = get_form_control_object(ctx, parent_form, fc->type, fs);

				if (fcobj)
					object_to_jsval(ctx, rval, fcobj);
			}
			break;
		}
	}

	return JS_TRUE;
}



static JSBool form_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static JSBool form_set_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
static void form_finalize(JSContext *ctx, JSObject *obj);

/* Each @form_class object must have a @document_class parent.  */
static const JSClass form_class = {
	"form",
	JSCLASS_HAS_PRIVATE,	/* struct form_view *, or NULL if detached */
	JS_PropertyStub, JS_PropertyStub,
	form_get_property, form_set_property,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, form_finalize
};

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

static const spidermonkeyFunctionSpec form_funcs[] = {
	{ "reset",	form_reset,	0 },
	{ "submit",	form_submit,	0 },
	{ NULL }
};

static struct form_view *
form_get_form_view(JSContext *ctx, JSObject *jsform, jsval *argv)
{
	struct form_view *fv = JS_GetInstancePrivate(ctx, jsform,
						     (JSClass *) &form_class,
						     argv);

	if (!fv) return NULL;	/* detached */

	assert(fv->ecmascript_obj == jsform);
	if_assert_failed return NULL;
	
	return fv;
}

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

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_class, NULL))
		return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, obj, NULL);
	if (!fv) return JS_FALSE; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	if (JSVAL_IS_STRING(id)) {
		struct form_control *fc;
		unsigned char *string;

		string = jsval_to_string(ctx, &id);
		foreach (fc, form->items) {
			JSObject *fcobj = NULL;
			struct form_state *fs;

			if ((!fc->id || c_strcasecmp(string, fc->id))
			    && (!fc->name || c_strcasecmp(string, fc->name)))
				continue;

			undef_to_jsval(ctx, vp);
			fs = find_form_state(doc_view, fc);
			if (fs) {
				fcobj = get_form_control_object(ctx, obj, fc->type, fs);
				if (fcobj)
					object_to_jsval(ctx, vp, fcobj);
			}
			break;
		}
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	undef_to_jsval(ctx, vp);

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		string_to_jsval(ctx, vp, form->action);
		break;

	case JSP_FORM_ELEMENTS:
	{
		/* jsform ('form') is form_elements' parent; who knows is that's correct */
		JSObject *jsform_elems = JS_NewObject(ctx, (JSClass *) &form_elements_class, NULL, obj);

		JS_DefineProperties(ctx, jsform_elems, (JSPropertySpec *) form_elements_props);
		spidermonkey_DefineFunctions(ctx, jsform_elems,
					     form_elements_funcs);
		object_to_jsval(ctx, vp, jsform_elems);
		/* SM will cache this property value for us so we create this
		 * just once per form. */
	}
		break;

	case JSP_FORM_ENCODING:
		switch (form->method) {
		case FORM_METHOD_GET:
		case FORM_METHOD_POST:
			string_to_jsval(ctx, vp, "application/x-www-form-urlencoded");
			break;
		case FORM_METHOD_POST_MP:
			string_to_jsval(ctx, vp, "multipart/form-data");
			break;
		case FORM_METHOD_POST_TEXT_PLAIN:
			string_to_jsval(ctx, vp, "text/plain");
			break;
		}
		break;

	case JSP_FORM_LENGTH:
		int_to_jsval(ctx, vp, list_size(&form->items));
		break;

	case JSP_FORM_METHOD:
		switch (form->method) {
		case FORM_METHOD_GET:
			string_to_jsval(ctx, vp, "GET");
			break;

		case FORM_METHOD_POST:
		case FORM_METHOD_POST_MP:
		case FORM_METHOD_POST_TEXT_PLAIN:
			string_to_jsval(ctx, vp, "POST");
			break;
		}
		break;

	case JSP_FORM_NAME:
		string_to_jsval(ctx, vp, form->name);
		break;

	case JSP_FORM_TARGET:
		string_to_jsval(ctx, vp, form->target);
		break;

	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case
		 * and leave *@vp unchanged.  Do the same here.
		 * (Actually not quite the same, as we already used
		 * @undef_to_jsval.)  */
		break;
	}

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
	unsigned char *string;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_class, NULL))
		return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, obj, NULL);
	if (!fv) return JS_FALSE; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORM_ACTION:
		string = stracpy(jsval_to_string(ctx, vp));
		if (form->action) {
			ecmascript_set_action(&form->action, string);
		} else {
			mem_free_set(&form->action, string);
		}
		break;

	case JSP_FORM_ENCODING:
		string = jsval_to_string(ctx, vp);
		if (!c_strcasecmp(string, "application/x-www-form-urlencoded")) {
			form->method = form->method == FORM_METHOD_GET ? FORM_METHOD_GET
			                                               : FORM_METHOD_POST;
		} else if (!c_strcasecmp(string, "multipart/form-data")) {
			form->method = FORM_METHOD_POST_MP;
		} else if (!c_strcasecmp(string, "text/plain")) {
			form->method = FORM_METHOD_POST_TEXT_PLAIN;
		}
		break;

	case JSP_FORM_METHOD:
		string = jsval_to_string(ctx, vp);
		if (!c_strcasecmp(string, "GET")) {
			form->method = FORM_METHOD_GET;
		} else if (!c_strcasecmp(string, "POST")) {
			form->method = FORM_METHOD_POST;
		}
		break;

	case JSP_FORM_NAME:
		mem_free_set(&form->name, stracpy(jsval_to_string(ctx, vp)));
		break;

	case JSP_FORM_TARGET:
		mem_free_set(&form->target, stracpy(jsval_to_string(ctx, vp)));
		break;

	default:
		/* Unrecognized integer property ID; someone is using
		 * the object as an array.  SMJS builtin classes (e.g.
		 * js_RegExpClass) just return JS_TRUE in this case.
		 * Do the same here.  */
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

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_class, argv)) return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	fv = form_get_form_view(ctx, obj, argv);
	if (!fv) return JS_FALSE; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);

	do_reset_form(doc_view, form);
	draw_forms(doc_view->session->tab->term, doc_view);

	boolean_to_jsval(ctx, rval, 0);

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

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &form_class, argv)) return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	ses = doc_view->session;
	fv = form_get_form_view(ctx, obj, argv);
	if (!fv) return JS_FALSE; /* detached */
	form = find_form_by_form_view(doc_view->document, fv);

	assert(form);
	submit_given_form(ses, doc_view, form, 0);

	boolean_to_jsval(ctx, rval, 0);

	return JS_TRUE;
}

JSObject *
get_form_object(JSContext *ctx, JSObject *jsdoc, struct form_view *fv)
{
	JSObject *jsform = fv->ecmascript_obj;

	if (jsform) {
		/* This assumes JS_GetInstancePrivate cannot GC.  */
		assert(JS_GetInstancePrivate(ctx, jsform,
					     (JSClass *) &form_class, NULL)
		       == fv);
		if_assert_failed return NULL;

		return jsform;
	}

	/* jsdoc ('document') is fv's parent */
	/* FIXME: That is NOT correct since the real containing element
	 * should be its parent, but gimme DOM first. --pasky */
	jsform = JS_NewObject(ctx, (JSClass *) &form_class, NULL, jsdoc);
	if (jsform == NULL)
		return NULL;
	JS_DefineProperties(ctx, jsform, (JSPropertySpec *) form_props);
	spidermonkey_DefineFunctions(ctx, jsform, form_funcs);

	if (!JS_SetPrivate(ctx, jsform, fv)) /* to @form_class */
		return NULL;
	fv->ecmascript_obj = jsform;
	return jsform;
}

static void
form_finalize(JSContext *ctx, JSObject *jsform)
{
	struct form_view *fv = JS_GetInstancePrivate(ctx, jsform,
						     (JSClass *) &form_class,
						     NULL);

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
		/* This assumes JS_GetInstancePrivate and JS_SetPrivate
		 * cannot GC.  */

		/* If this assertion fails, it is not clear whether
		 * the private pointer of jsform should be reset;
		 * crashes seem possible either way.  Resetting it is
		 * easiest.  */
		assert(JS_GetInstancePrivate(spidermonkey_empty_context,
					     jsform,
					     (JSClass *) &form_class, NULL)
		       == fv);
		if_assert_failed {}

		JS_SetPrivate(spidermonkey_empty_context, jsform, NULL);
		fv->ecmascript_obj = NULL;
	}
}


static JSBool forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

/* Each @forms_class object must have a @document_class parent.  */
const JSClass forms_class = {
	"forms",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub,
	forms_get_property, JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

static JSBool forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool forms_namedItem(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

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
const JSPropertySpec forms_props[] = {
	{ "length",	JSP_FORMS_LENGTH,	JSPROP_ENUMERATE | JSPROP_READONLY},
	{ NULL }
};

/* Find the form whose name is @name, which should normally be a
 * string (but might not be).  If found, set *rval = the DOM
 * object.  If not found, leave *rval unchanged.  */
static void
find_form_by_name(JSContext *ctx, JSObject *jsdoc,
		  struct document_view *doc_view,
		  jsval name, jsval *rval)
{
	unsigned char *string = jsval_to_string(ctx, &name);
	struct form *form;

	if (!*string)
		return;

	foreach (form, doc_view->document->forms) {
		if (form->name && !c_strcasecmp(string, form->name)) {
			object_to_jsval(ctx, rval, get_form_object(ctx, jsdoc,
					find_form_view(doc_view, form)));
			break;
		}
	}
}

/* @forms_class.getProperty */
static JSBool
forms_get_property(JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;

	/* This can be called if @obj if not itself an instance of the
	 * appropriate class but has one in its prototype chain.  Fail
	 * such calls.  */
	if (!JS_InstanceOf(ctx, obj, (JSClass *) &forms_class, NULL))
		return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;
	document = doc_view->document;

	if (JSVAL_IS_STRING(id)) {
		/* When SMJS evaluates forms.namedItem("foo"), it first
		 * calls forms_get_property with id = JSString "namedItem"
		 * and *vp = JSObject JSFunction forms_namedItem.
		 * If we don't find a form whose name is id,
		 * we must leave *vp unchanged here, to avoid
		 * "TypeError: forms.namedItem is not a function".  */
		find_form_by_name(ctx, parent_doc, doc_view, id, vp);
		return JS_TRUE;
	}

	if (!JSVAL_IS_INT(id))
		return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case JSP_FORMS_LENGTH:
		int_to_jsval(ctx, vp, list_size(&document->forms));
		break;
	default:
		/* Array index. */
		forms_item(ctx, obj, 1, &id, vp);
		break;
	}

	return JS_TRUE;
}

/* @forms_funcs{"item"} */
static JSBool
forms_item(JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *parent_doc;	/* instance of @document_class */
	JSObject *parent_win;	/* instance of @window_class */
	struct view_state *vs;
	struct form_view *fv;
	int counter = -1;
	int index;

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &forms_class, argv)) return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);

	if (argc != 1)
		return JS_TRUE;

	if (!JS_ValueToInt32(ctx, argv[0], &index))
		return JS_FALSE;
	undef_to_jsval(ctx, rval);

	foreach (fv, vs->forms) {
		counter++;
		if (counter == index) {
			object_to_jsval(ctx, rval, get_form_object(ctx, parent_doc, fv));
			break;
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

	if (!JS_InstanceOf(ctx, obj, (JSClass *) &forms_class, argv)) return JS_FALSE;
	parent_doc = JS_GetParent(ctx, obj);
	assert(JS_InstanceOf(ctx, parent_doc, (JSClass *) &document_class, NULL));
	if_assert_failed return JS_FALSE;
	parent_win = JS_GetParent(ctx, parent_doc);
	assert(JS_InstanceOf(ctx, parent_win, (JSClass *) &window_class, NULL));
	if_assert_failed return JS_FALSE;

	vs = JS_GetInstancePrivate(ctx, parent_win,
				   (JSClass *) &window_class, NULL);
	doc_view = vs->doc_view;

	if (argc != 1)
		return JS_TRUE;

	undef_to_jsval(ctx, rval);
	find_form_by_name(ctx, parent_doc, doc_view, argv[0], rval);
	return JS_TRUE;
}


static JSString *
unicode_to_jsstring(JSContext *ctx, unicode_val_T u)
{
	jschar buf[2];

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
jsval_to_accesskey(JSContext *ctx, jsval *vp)
{
	size_t len;
	const jschar *chr;

	/* Convert the value in place, to protect the result from GC.  */
	if (JS_ConvertValue(ctx, *vp, JSTYPE_STRING, vp) == JS_FALSE)
		return UCS_NO_CHAR;
	len = JS_GetStringLength(JSVAL_TO_STRING(*vp));
	chr = JS_GetStringChars(JSVAL_TO_STRING(*vp));

	/* This implementation ignores extra characters in the string.  */
	if (len < 1)
		return 0;	/* which means no access key */
	if (!is_utf16_surrogate(chr[0]))
		return chr[0];
	if (len >= 2
	    && is_utf16_high_surrogate(chr[0])
	    && is_utf16_low_surrogate(chr[1]))
		return join_utf16_surrogates(chr[0], chr[1]);
	JS_ReportError(ctx, "Invalid UTF-16 sequence");
	return UCS_NO_CHAR;	/* which the caller will reject */
}
