/* The QuickJS input objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>

#include "elinks.h"

#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/document.h"
#include "ecmascript/quickjs/form.h"
#include "ecmascript/quickjs/forms.h"
#include "ecmascript/quickjs/input.h"
#include "ecmascript/quickjs/window.h"
#include "intl/charsets.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSClassID js_input_class_id;

void *map_inputs;
//static std::map<struct form_state *, JSValueConst> map_inputs;

JSValue getInput(JSContext *ctx, struct form_state *fs);


/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static struct form_state *js_input_get_form_state(JSContext *ctx, JSValueConst jsinput);

struct JSString {
    uint32_t header; /* must come first, 32-bit */
    uint32_t len : 31;
    uint8_t is_wide_char : 1; /* 0 = 8 bits, 1 = 16 bits characters */
    /* for JS_ATOM_TYPE_SYMBOL: hash = 0, atom_type = 3,
       for JS_ATOM_TYPE_PRIVATE: hash = 1, atom_type = 3
       XXX: could change encoding to have one more bit in hash */
    uint32_t hash : 30;
    uint8_t atom_type : 2; /* != 0 if atom, JS_ATOM_TYPE_x */
    uint32_t hash_next; /* atom_index for JS_ATOM_TYPE_SYMBOL */
#ifdef DUMP_LEAKS
    struct list_head link; /* string list */
#endif
    union {
        uint8_t str8[0]; /* 8 bit strings will get an extra null terminator */
        uint16_t str16[0];
    } u;
};

typedef struct JSString JSString;

static JSValue
unicode_to_value(JSContext *ctx, unicode_val_T u)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue str = JS_NewStringLen(ctx, "        ", 8);
	REF_JS(str);

	JSString *p = JS_VALUE_GET_STRING(str);
	p->is_wide_char = 1;

	if (u <= 0xFFFF && !is_utf16_surrogate(u)) {
		p->u.str16[0] = u;
		p->len = 1;
		return str;
	} else if (needs_utf16_surrogates(u)) {
		p->u.str16[0] = get_utf16_high_surrogate(u);
		p->u.str16[1] = get_utf16_low_surrogate(u);
		p->len = 2;
		return str;
	} else {
		p->len = 1;
		p->u.str16[0] = 0;
		return str;
	}
}

static int
string_get(const JSString *p, int idx)
{
	return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

/* Convert the string *@vp to an access key.  Return 0 for no access
 * key, UCS_NO_CHAR on error, or the access key otherwise.  */
static unicode_val_T
js_value_to_accesskey(JSValueConst val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(val);

	JSString *p = JS_VALUE_GET_STRING(val);

	size_t len;
	char16_t chr[2];

	len = p->len;

	/* This implementation ignores extra characters in the string.  */
	if (len < 1)
		return 0;	/* which means no access key */
	chr[0] = string_get(p, 0);

	if (!is_utf16_surrogate(chr[0])) {
		return chr[0];
	}
	if (len >= 2) {
		chr[1] = string_get(p, 1);
		if (is_utf16_high_surrogate(chr[0])
			&& is_utf16_low_surrogate(chr[1])) {
			return join_utf16_surrogates(chr[0], chr[1]);
		}
	}
	return UCS_NO_CHAR;	/* which the caller will reject */
}

static JSValue
js_input_get_property_accessKey(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
		JSValue r = JS_NewStringLen(ctx, "", 0);
		RETURN_JS(r);
	} else {
		JSValue vv = unicode_to_value(ctx, link->accesskey);
		RETURN_JS(vv);
	}
	return JS_UNDEFINED;
}

static JSValue
js_input_set_property_accessKey(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	unicode_val_T accesskey;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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

	if (!JS_IsString(val)) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL;
	}
	accesskey = js_value_to_accesskey(val);

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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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

	JSValue r = JS_NewString(ctx, fc->alt);
	RETURN_JS(r);
}

static JSValue
js_input_set_property_alt(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	JSValue r = JS_NewString(ctx, fc->default_value);
	RETURN_JS(r);
}

static JSValue
js_input_get_property_disabled(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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

	JSValue r = JS_NewString(ctx, fc->name);
	RETURN_JS(r);
}

/* @input_class.setProperty */
static JSValue
js_input_set_property_name(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
		JSValue r = JS_NewString(ctx, link->where_img);
		RETURN_JS(r);
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
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct link *link = NULL;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	const char *s = NULL;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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

	JSValue r = JS_NewString(ctx, s);
	RETURN_JS(r);
}

static JSValue
js_input_get_property_value(JSContext *ctx, JSValueConst this_val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct form_state *fs;
	fs = js_input_get_form_state(ctx, this_val);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		return JS_NULL; /* detached */
	}

	JSValue r = JS_NewString(ctx, fs->value);
	RETURN_JS(r);
}

static JSValue
js_input_set_property_value(JSContext *ctx, JSValueConst this_val, JSValue val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);
	REF_JS(val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
		const char *str;
		char *string;
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


static struct form_state *
js_input_get_form_state(JSContext *ctx, JSValueConst jsinput)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(jsinput);

	struct form_state *fs = (struct form_state *)JS_GetOpaque(jsinput, js_input_class_id);

	return fs;
}

/* @input_funcs{"blur"} */
static JSValue
js_input_blur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

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
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
js_input_focus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct session *ses;
	struct form_state *fs;
	struct el_form_control *fc;
	int linknum;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)JS_GetContextOpaque(ctx);
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
js_input_select(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	return JS_UNDEFINED;
}

JSValue
js_get_input_object(JSContext *ctx, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	return getInput(ctx, fs);
}

static JSValue
js_input_toString(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	REF_JS(this_val);

	return JS_NewString(ctx, "[input object]");
}


static const JSCFunctionListEntry js_input_proto_funcs[] = {
	JS_CGETSET_DEF("accessKey",	js_input_get_property_accessKey, js_input_set_property_accessKey),
	JS_CGETSET_DEF("alt",	js_input_get_property_alt, js_input_set_property_alt),
	JS_CGETSET_DEF("checked",	js_input_get_property_checked, js_input_set_property_checked),
	JS_CGETSET_DEF("defaultChecked", js_input_get_property_defaultChecked, NULL),
	JS_CGETSET_DEF("defaultValue",js_input_get_property_defaultValue, NULL),
	JS_CGETSET_DEF("disabled",	js_input_get_property_disabled, js_input_set_property_disabled),
	JS_CGETSET_DEF("form",	js_input_get_property_form, NULL),
	JS_CGETSET_DEF("maxLength",	js_input_get_property_maxLength, js_input_set_property_maxLength),
	JS_CGETSET_DEF("name",	js_input_get_property_name, js_input_set_property_name),
	JS_CGETSET_DEF("readonly",	js_input_get_property_readonly, js_input_set_property_readonly),
	JS_CGETSET_DEF("selectedIndex", js_input_get_property_selectedIndex, js_input_set_property_selectedIndex),
	JS_CGETSET_DEF("size",	js_input_get_property_size, NULL),
	JS_CGETSET_DEF("src",	js_input_get_property_src, js_input_set_property_src),
	JS_CGETSET_DEF("tabindex",	js_input_get_property_tabIndex, NULL),
	JS_CGETSET_DEF("type",	js_input_get_property_type, NULL),
	JS_CGETSET_DEF("value",	js_input_get_property_value, js_input_set_property_value),
	JS_CFUNC_DEF("blur", 0 , js_input_blur),
	JS_CFUNC_DEF("click", 0 , js_input_click),
	JS_CFUNC_DEF("focus", 0 , js_input_focus),
	JS_CFUNC_DEF("select", 0 , js_input_select),
	JS_CFUNC_DEF("toString", 0, js_input_toString)
};

void
quickjs_detach_form_state(struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue jsinput = fs->ecmascript_obj;

	if (!JS_IsNull(jsinput)) {
		attr_erase_from_map(map_inputs, (void *)fs);
		JS_SetOpaque(jsinput, NULL);
		fs->ecmascript_obj = JS_NULL;
	}
}

void
quickjs_moved_form_state(struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue jsinput = fs->ecmascript_obj;
	REF_JS(jsinput);

	if (!JS_IsNull(jsinput)) {
		attr_erase_from_map(map_inputs, (void *)fs);
		JS_SetOpaque(jsinput, fs);
		attr_save_in_map(map_inputs, (void *)fs, jsinput);
	}
}

static
void js_input_finalizer(JSRuntime *rt, JSValue val)
{
	REF_JS(val);

	struct form_state *fs = (struct form_state *)JS_GetOpaque(val, js_input_class_id);

	if (fs) {
		fs->ecmascript_obj = JS_NULL;
		attr_erase_from_map(map_inputs, (void *)fs);
	}
}


static JSClassDef js_input_class = {
	"input",
	js_input_finalizer
};

static JSValue
js_input_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	REF_JS(new_target);

	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	/* using new_target to get the prototype is necessary when the
	 class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	REF_JS(proto);

	if (JS_IsException(proto)) {
		goto fail;
	}
	obj = JS_NewObjectProtoClass(ctx, proto, js_input_class_id);
	JS_FreeValue(ctx, proto);

	if (JS_IsException(obj)) {
		goto fail;
	}
	RETURN_JS(obj);

fail:
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

int
js_input_init(JSContext *ctx, JSValue global_obj)
{
	REF_JS(global_obj);

	JSValue input_proto, input_class;

	/* create the input class */
	JS_NewClassID(&js_input_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_input_class_id, &js_input_class);

	input_proto = JS_NewObject(ctx);
	REF_JS(input_proto);

	JS_SetPropertyFunctionList(ctx, input_proto, js_input_proto_funcs, countof(js_input_proto_funcs));

	input_class = JS_NewCFunction2(ctx, js_input_ctor, "input", 0, JS_CFUNC_constructor, 0);
	REF_JS(input_class);

	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, input_class, input_proto);
	JS_SetClassProto(ctx, js_input_class_id, input_proto);

	JS_SetPropertyStr(ctx, global_obj, "input", JS_DupValue(ctx, input_proto));
	return 0;
}

JSValue
getInput(JSContext *ctx, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	JSValue second;
	static int initialized;
	if (!initialized) {
		JS_NewClassID(&js_input_class_id);
		JS_NewClass(JS_GetRuntime(ctx), js_input_class_id, &js_input_class);
		initialized = 1;
		map_inputs = attr_create_new_input_map();
	}
	second = attr_find_in_map(map_inputs, (void *)fs);

	if (!JS_IsNull(second)) {
		JSValue r = JS_DupValue(ctx, second);
		RETURN_JS(r);
	}
	JSValue input_obj = JS_NewObjectClass(ctx, js_input_class_id);

	JS_SetPropertyFunctionList(ctx, input_obj, js_input_proto_funcs, countof(js_input_proto_funcs));
	JS_SetClassProto(ctx, js_input_class_id, input_obj);
	JS_SetOpaque(input_obj, fs);
	fs->ecmascript_obj = input_obj;
	attr_save_in_map(map_inputs, (void *)fs, input_obj);

	JSValue rr = JS_DupValue(ctx, input_obj);
	RETURN_JS(rr);
}
