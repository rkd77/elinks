/* The MuJS input objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/form.h"
#include "ecmascript/mujs/forms.h"
#include "ecmascript/mujs/input.h"
#include "ecmascript/mujs/window.h"
#include "intl/charsets.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

void *map_inputs;
//static std::map<struct form_state *, void *> map_inputs;

/* Accordingly to the JS specs, each input type should own object. That'd be a
 * huge PITA though, however DOM comes to the rescue and defines just a single
 * HTMLInputElement. The difference could be spotted only by some clever tricky
 * JS code, but I hope it doesn't matter anywhere. --pasky */

static struct form_state *mjs_input_get_form_state(js_State *J);

static char *
mjs_unicode_to_string(unicode_val_T v)
{
	return encode_utf8(v);
}

/* Convert the string *@vp to an access key.  Return 0 for no access
 * key, UCS_NO_CHAR on error, or the access key otherwise.  */
static unicode_val_T
mjs_value_to_accesskey(const char *val)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (*val) {
		const char *end = strchr(val, '\0');
		char *begin = (char *)val;

		return utf8_to_unicode(&begin, end);
	}
	return 0;
}

static void
mjs_input_get_property_accessKey(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (!link) {
		js_pushundefined(J);
		return;
	}

	if (!link->accesskey) {
		js_pushstring(J, "");
		return;
	} else {
		js_pushstring(J, mjs_unicode_to_string(link->accesskey));
		return;
	}
	js_pushundefined(J);
}

static void
mjs_input_set_property_accessKey(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	const char *val = js_tostring(J, 1);

	accesskey = mjs_value_to_accesskey(val);

	if (link) {
		link->accesskey = accesskey;
	}
	js_pushundefined(J);
}

static void
mjs_input_get_property_alt(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushstring(J, fc->alt);
}

static void
mjs_input_set_property_alt(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	const char *str = js_tostring(J, 1);
	char *string;

	if (!str) {
		js_error(J, "!str");
		return;
	}
	string = stracpy(str);
	mem_free_set(&fc->alt, string);
	js_pushundefined(J);
}

static void
mjs_input_get_property_checked(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs;
	fs = mjs_input_get_form_state(J);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	js_pushboolean(J, fs->state);
}

static void
mjs_input_set_property_checked(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO) {
		js_pushundefined(J);
		return;
	}
	fs->state = js_toboolean(J, 1);
	js_pushundefined(J);
}

static void
mjs_input_get_property_defaultChecked(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);
	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushboolean(J, fc->default_state);
}

static void
mjs_input_get_property_defaultValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushstring(J, fc->default_value);
	/* FIXME (bug 805): convert from the charset of the document */
}

static void
mjs_input_get_property_disabled(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	/* FIXME: <input readonly disabled> --pasky */
	js_pushboolean(J, fc->mode == FORM_MODE_DISABLED);
}

static void
mjs_input_set_property_disabled(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	int val = js_toboolean(J, 1);

	/* FIXME: <input readonly disabled> --pasky */
	fc->mode = (val ? FORM_MODE_DISABLED
		: fc->mode == FORM_MODE_READONLY ? FORM_MODE_READONLY
		: FORM_MODE_NORMAL);

	js_pushundefined(J);
}

static void
mjs_input_get_property_form(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	// TODO
	js_pushundefined(J);
}

static void
mjs_input_get_property_maxLength(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushnumber(J, fc->maxlength);
}

static void
mjs_input_set_property_maxLength(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	fc->maxlength = js_toint32(J, 1);
	js_pushundefined(J);
}

static void
mjs_input_get_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushstring(J, fc->name);
}

/* @input_class.setProperty */
static void
mjs_input_set_property_name(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	const char *str = js_tostring(J, 1);
	char *string;

	if (!str) {
		js_error(J, "!str");
		return;
	}

	string = stracpy(str);
	mem_free_set(&fc->name, string);
	js_pushundefined(J);
}

static void
mjs_input_get_property_readonly(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushboolean(J, fc->mode == FORM_MODE_READONLY);
	/* FIXME: <input readonly disabled> --pasky */
}

/* @input_class.setProperty */
static void
mjs_input_set_property_readonly(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	int val = js_toint32(J, 1);

	/* FIXME: <input readonly disabled> --pasky */
	fc->mode = (val ? FORM_MODE_READONLY
	                      : fc->mode == FORM_MODE_DISABLED ? FORM_MODE_DISABLED
	                                                       : FORM_MODE_NORMAL);

	js_pushundefined(J);
}

static void
mjs_input_get_property_selectedIndex(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type == FC_SELECT) {
		js_pushnumber(J, fs->state);
		return;
	} else {
		js_pushundefined(J);
		return;
	}
}

/* @input_class.setProperty */
static void
mjs_input_set_property_selectedIndex(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type == FC_SELECT) {
		int item = js_toint32(J, 1);

		if (item >= 0 && item < fc->nvalues) {
			fs->state = item;
			mem_free_set(&fs->value, stracpy(fc->values[item]));
		}
	}
	js_pushundefined(J);
}

static void
mjs_input_get_property_size(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	js_pushnumber(J, fc->size);
}

static void
mjs_input_get_property_src(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);
	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link && link->where_img) {
		js_pushstring(J, link->where_img);
		return;
	} else {
		js_pushundefined(J);
		return;
	}
}

static void
mjs_input_set_property_src(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
		js_pushundefined(J);
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		const char *str = js_tostring(J, 1);
		char *string;

		if (!str) {
			js_error(J, "!str");
			return;
		}

		string = stracpy(str);
		mem_free_set(&link->where_img, string);
	}
	js_pushundefined(J);
}

static void
mjs_input_get_property_tabIndex(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum >= 0) link = &document->links[linknum];

	if (link) {
		/* FIXME: This is WRONG. --pasky */
		js_pushnumber(J, link->number);
		return;
	} else {
		js_pushundefined(J);
		return;
	}
}

static void
mjs_input_get_property_type(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	const char *s = NULL;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
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

	js_pushstring(J, s);
}

static void
mjs_input_get_property_value(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	js_pushstring(J, fs->value);
}

static void
mjs_input_set_property_value(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct view_state *vs;
	struct document_view *doc_view;
	struct document *document;
	struct form_state *fs;
	struct el_form_control *fc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;

	if (!vs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_error(J, "!vs");
		return;
	}
	doc_view = vs->doc_view;
	document = doc_view->document;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}
	fc = find_form_control(document, fs);

	assert(fc);
	assert(fc->form && fs);

	if (fc->type != FC_FILE) {
		const char *str = js_tostring(J, 1);
		char *string;

		if (!str) {
			js_error(J, "!str");
			return;
		}

		string = stracpy(str);

		mem_free_set(&fs->value, string);
		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD)
			fs->state = strlen(fs->value);
	}
	js_pushundefined(J);
}

static struct form_state *
mjs_input_get_form_state(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct form_state *fs = (struct form_state *)js_touserdata(J, 0, "input");

	return fs;
}

/* @input_funcs{"blur"} */
static void
mjs_input_blur(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We are a text-mode browser and there *always* has to be something
	 * selected.  So we do nothing for now. (That was easy.) */
	js_pushundefined(J);
}

/* @input_funcs{"click"} */
static void
mjs_input_click(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0) {
		js_pushundefined(J);
		return;
	}

	/* Restore old current_link afterwards? */
	jump_to_link_number(ses, doc_view, linknum);
	if (enter(ses, doc_view, 0) == FRAME_EVENT_REFRESH) {
		refresh_view(ses, doc_view, 0);
	} else {
		print_screen_status(ses);
	}
	js_pushundefined(J);
}

/* @input_funcs{"focus"} */
static void
mjs_input_focus(js_State *J)
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
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	vs = interpreter->vs;
	doc_view = vs->doc_view;
	document = doc_view->document;
	ses = doc_view->session;
	fs = mjs_input_get_form_state(J);

	if (!fs) {
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s %d\n", __FILE__, __FUNCTION__, __LINE__);
#endif
		js_pushnull(J);
		return; /* detached */
	}

	assert(fs);
	fc = find_form_control(document, fs);
	assert(fc);

	linknum = get_form_control_link(document, fc);
	/* Hiddens have no link. */
	if (linknum < 0) {
		js_pushundefined(J);
		return;
	}

	jump_to_link_number(ses, doc_view, linknum);
	js_pushundefined(J);
}

/* @input_funcs{"select"} */
static void
mjs_input_select(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	/* We support no text selecting yet.  So we do nothing for now.
	 * (That was easy, too.) */
	js_pushundefined(J);
}

static void
mjs_input_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[input object]");
}

static void
mjs_input_finalizer(js_State *J, void *node)
{
	struct form_state *fs = (struct form_state *)node;
	attr_erase_from_map(map_inputs, fs);
}

void
mjs_push_input_object(js_State *J, struct form_state *fs)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif

	js_newobject(J);
	{
		js_newuserdata(J, "input", fs, mjs_input_finalizer);

		addmethod(J, "blur", mjs_input_blur, 0);
		addmethod(J, "click", mjs_input_click, 0);
		addmethod(J, "focus", mjs_input_focus, 0);
		addmethod(J, "select", mjs_input_select, 0);
		addmethod(J, "toString", mjs_input_toString, 0);

		addproperty(J, "accessKey",	mjs_input_get_property_accessKey, mjs_input_set_property_accessKey);
		addproperty(J, "alt",	mjs_input_get_property_alt, mjs_input_set_property_alt);
		addproperty(J, "checked",	mjs_input_get_property_checked, mjs_input_set_property_checked);
		addproperty(J, "defaultChecked", mjs_input_get_property_defaultChecked, NULL);
		addproperty(J, "defaultValue",mjs_input_get_property_defaultValue, NULL);
		addproperty(J, "disabled",	mjs_input_get_property_disabled, mjs_input_set_property_disabled);
		addproperty(J, "form",	mjs_input_get_property_form, NULL);
		addproperty(J, "maxLength",	mjs_input_get_property_maxLength, mjs_input_set_property_maxLength);
		addproperty(J, "name",	mjs_input_get_property_name, mjs_input_set_property_name);
		addproperty(J, "readonly",	mjs_input_get_property_readonly, mjs_input_set_property_readonly);
		addproperty(J, "selectedIndex", mjs_input_get_property_selectedIndex, mjs_input_set_property_selectedIndex);
		addproperty(J, "size",	mjs_input_get_property_size, NULL);
		addproperty(J, "src",	mjs_input_get_property_src, mjs_input_set_property_src);
		addproperty(J, "tabindex",	mjs_input_get_property_tabIndex, NULL);
		addproperty(J, "type",	mjs_input_get_property_type, NULL);
		addproperty(J, "value",	mjs_input_get_property_value, mjs_input_set_property_value);
	}
	attr_save_in_map(map_inputs, fs, fs);
}
