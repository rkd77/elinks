/* HTML forms parser */
/* $Id: forms.c,v 1.60.2.4 2005/04/05 21:08:41 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "document/html/parser/forms.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/forms.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"



void
html_form(unsigned char *a)
{
	unsigned char *al;
	struct form *form;

	html_context.was_br = 1;

	form = init_form();
	if (!form) return;

	form->method = FORM_METHOD_GET;
	form->form_num = a - html_context.startf;

	al = get_attr_val(a, "method");
	if (al) {
		if (!strcasecmp(al, "post")) {
			unsigned char *enctype = get_attr_val(a, "enctype");

			form->method = FORM_METHOD_POST;
			if (enctype) {
				if (!strcasecmp(enctype, "multipart/form-data"))
					form->method = FORM_METHOD_POST_MP;
				if (!strcasecmp(enctype, "text/plain"))
					form->method = FORM_METHOD_POST_TEXT_PLAIN;
				mem_free(enctype);
			}
		}
		mem_free(al);
	}
	al = get_attr_val(a, "name");
	if (al) form->name = al;

	al = get_attr_val(a, "action");
	/* The HTML specification at
	 * http://www.w3.org/TR/REC-html40/interact/forms.html#h-17.3 states
	 * that the behavior of an empty action attribute should be undefined.
	 * Mozilla handles action="" as action="<current-URI>" which seems
	 * reasonable. (bug 615) */
	if (al && *al) {
		form->action = join_urls(html_context.base_href, trim_chars(al, ' ', 0));
		mem_free(al);

	} else {
		enum uri_component components = URI_ORIGINAL;

		mem_free_if(al);

		/* We have to do following for GET method, because we would end
		 * up with two '?' otherwise. */
		if (form->method == FORM_METHOD_GET)
			components = URI_FORM_GET;

		form->action = get_uri_string(html_context.base_href, components);

		/* No action URI should contain post data */
		assert(!form->action || !strchr(form->action, POST_CHAR));

		/* GET method URIs should not have '?'. */
		assert(!form->action
			|| form->method != FORM_METHOD_GET
			|| !strchr(form->action, '?'));
	}

	al = get_target(a);
	form->target = al ? al : stracpy(html_context.base_target);

	html_context.special_f(html_context.part, SP_FORM, form);
}


int
get_form_mode(unsigned char *attr)
{
	if (has_attr(attr, "disabled")) return FORM_MODE_DISABLED;
	if (has_attr(attr, "readonly")) return FORM_MODE_READONLY;
	return FORM_MODE_NORMAL;
}

static struct form_control *
init_form_control(enum form_type type, unsigned char *attr)
{
	struct form_control *fc;

	fc = mem_calloc(1, sizeof(*fc));
	if (!fc) return NULL;

	fc->type = type;
	fc->position = attr - html_context.startf;
	fc->mode = get_form_mode(attr);

	return fc;
}

void
html_button(unsigned char *a)
{
	unsigned char *al;
	struct form_control *fc;
	enum form_type type = FC_SUBMIT;

	html_focusable(a);

	al = get_attr_val(a, "type");
	if (!al) goto no_type_attr;

	if (!strcasecmp(al, "button")) {
		type = FC_BUTTON;
	} else if (!strcasecmp(al, "reset")) {
		type = FC_RESET;
	} else if (strcasecmp(al, "submit")) {
		/* unknown type */
		mem_free(al);
		return;
	}
	mem_free(al);

no_type_attr:
	fc = init_form_control(type, a);
	if (!fc) return;

	fc->name = get_attr_val(a, "name");
	fc->default_value = get_attr_val(a, "value");
	if (!fc->default_value && fc->type == FC_SUBMIT) fc->default_value = stracpy("Submit");
	if (!fc->default_value && fc->type == FC_RESET) fc->default_value = stracpy("Reset");
	if (!fc->default_value && fc->type == FC_BUTTON) fc->default_value = stracpy("Button");
	if (!fc->default_value) fc->default_value = stracpy("");

	/* XXX: Does this make sense here? Where do we get FC_IMAGE? */
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	html_context.special_f(html_context.part, SP_CONTROL, fc);
	format.form = fc;
	format.style.attr |= AT_BOLD;
}

void
html_input(unsigned char *a)
{
	int i;
	unsigned char *al;
	struct form_control *fc;
	enum form_type type = FC_TEXT;

	al = get_attr_val(a, "type");
	if (!al) goto no_type_attr;

	if (!strcasecmp(al, "text")) type = FC_TEXT;
	else if (!strcasecmp(al, "password")) type = FC_PASSWORD;
	else if (!strcasecmp(al, "checkbox")) type = FC_CHECKBOX;
	else if (!strcasecmp(al, "radio")) type = FC_RADIO;
	else if (!strcasecmp(al, "submit")) type = FC_SUBMIT;
	else if (!strcasecmp(al, "reset")) type = FC_RESET;
	else if (!strcasecmp(al, "button")) type = FC_BUTTON;
	else if (!strcasecmp(al, "file")) type = FC_FILE;
	else if (!strcasecmp(al, "hidden")) type = FC_HIDDEN;
	else if (!strcasecmp(al, "image")) type = FC_IMAGE;
	/* else unknown type, let it default to FC_TEXT. */
	mem_free(al);

no_type_attr:
	fc = init_form_control(type, a);
	if (!fc) return;

	fc->name = get_attr_val(a, "name");
	if (fc->type != FC_FILE) fc->default_value = get_attr_val(a, "value");
	if (!fc->default_value && fc->type == FC_CHECKBOX) fc->default_value = stracpy("on");
	if (!fc->default_value && fc->type == FC_SUBMIT) fc->default_value = stracpy("Submit");
	if (!fc->default_value && fc->type == FC_RESET) fc->default_value = stracpy("Reset");
	if (!fc->default_value && fc->type == FC_BUTTON) fc->default_value = stracpy("Button");
	if (!fc->default_value) fc->default_value = stracpy("");

	fc->size = get_num(a, "size");
	if (fc->size == -1) fc->size = global_doc_opts->default_form_input_size;
	fc->size++;
	if (fc->size > global_doc_opts->box.width)
		fc->size = global_doc_opts->box.width;
	fc->maxlength = get_num(a, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) fc->default_state = has_attr(a, "checked");
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	if (fc->type == FC_HIDDEN) goto hid;
#if 0 /* wait for better times --witekfl */
		format.form = fc;
		process_hidden_link(html_context.part);
		goto hid;
	}
#endif

	put_chrs(" ", 1, html_context.put_chars_f, html_context.part);
	html_stack_dup(ELEMENT_KILLABLE);
	html_focusable(a);
	format.form = fc;
	if (format.title) mem_free(format.title);
	format.title = get_attr_val(a, "title");
	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			format.style.attr |= AT_BOLD;
			for (i = 0; i < fc->size; i++)
				put_chrs("_", 1, html_context.put_chars_f, html_context.part);
			break;
		case FC_CHECKBOX:
			format.style.attr |= AT_BOLD;
			put_chrs("[&nbsp;]", 8, html_context.put_chars_f, html_context.part);
			break;
		case FC_RADIO:
			format.style.attr |= AT_BOLD;
			put_chrs("(&nbsp;)", 8, html_context.put_chars_f, html_context.part);
			break;
		case FC_IMAGE:
			mem_free_set(&format.image, NULL);
			al = get_url_val(a, "src");
			if (!al) al = get_url_val(a, "dynsrc");
			if (al) {
				format.image = join_urls(html_context.base_href, al);
				mem_free(al);
			}
			format.style.attr |= AT_BOLD;
			put_chrs("[&nbsp;", 7, html_context.put_chars_f, html_context.part);
			if (fc->alt)
				put_chrs(fc->alt, strlen(fc->alt), html_context.put_chars_f, html_context.part);
			else if (fc->name)
				put_chrs(fc->name, strlen(fc->name), html_context.put_chars_f, html_context.part);
			else
				put_chrs("Submit", 6, html_context.put_chars_f, html_context.part);

			put_chrs("&nbsp;]", 7, html_context.put_chars_f, html_context.part);
			break;
		case FC_SUBMIT:
		case FC_RESET:
		case FC_BUTTON:
			format.style.attr |= AT_BOLD;
			put_chrs("[&nbsp;", 7, html_context.put_chars_f, html_context.part);
			if (fc->default_value)
				put_chrs(fc->default_value, strlen(fc->default_value), html_context.put_chars_f, html_context.part);
			put_chrs("&nbsp;]", 7, html_context.put_chars_f, html_context.part);
			break;
		case FC_TEXTAREA:
		case FC_SELECT:
		case FC_HIDDEN:
			INTERNAL("bad control type");
	}
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, html_context.put_chars_f, html_context.part);

hid:
	html_context.special_f(html_context.part, SP_CONTROL, fc);
}

void
html_select(unsigned char *a)
{
	/* Note I haven't seen this code in use, do_html_select() seems to take
	 * care of bussiness. --FF */
	/* It gets called when the "multiple" attribute is set. --jonas */

	unsigned char *al = get_attr_val(a, "name");

	if (!al) return;
	html_focusable(a);
	html_top.type = ELEMENT_DONT_KILL;
	mem_free_set(&format.select, al);
	format.select_disabled = has_attr(a, "disabled") ? FORM_MODE_DISABLED : FORM_MODE_NORMAL;
}

void
html_option(unsigned char *a)
{
	struct form_control *fc;
	unsigned char *val;

	if (!format.select) return;

	val = get_attr_val(a, "value");
	if (!val) {
		struct string str;
		unsigned char *p, *r;
		unsigned char *name;
		int namelen;

		for (p = a - 1; *p != '<'; p--);

		if (!init_string(&str)) goto end_parse;
		if (parse_element(p, html_context.eoff, NULL, NULL, NULL, &p)) {
			INTERNAL("parse element failed");
			val = str.source;
			goto end_parse;
		}

se:
		while (p < html_context.eoff && isspace(*p)) p++;
		while (p < html_context.eoff && !isspace(*p) && *p != '<') {

sp:
			add_char_to_string(&str, *p ? *p : ' '), p++;
		}

		r = p;
		val = str.source; /* Has to be before the possible 'goto end_parse' */

		while (r < html_context.eoff && isspace(*r)) r++;
		if (r >= html_context.eoff) goto end_parse;
		if (r - 2 <= html_context.eoff && (r[1] == '!' || r[1] == '?')) {
			p = skip_comment(r, html_context.eoff);
			goto se;
		}
		if (parse_element(r, html_context.eoff, &name, &namelen, NULL, &p)) goto sp;
		if (strlcasecmp(name, namelen, "OPTION", 6)
		    && strlcasecmp(name, namelen, "/OPTION", 7)
		    && strlcasecmp(name, namelen, "SELECT", 6)
		    && strlcasecmp(name, namelen, "/SELECT", 7)
		    && strlcasecmp(name, namelen, "OPTGROUP", 8)
		    && strlcasecmp(name, namelen, "/OPTGROUP", 9))
			goto se;
	}

end_parse:
	fc = init_form_control(FC_CHECKBOX, a);
	if (!fc) {
		mem_free_if(val);
		return;
	}

	fc->name = null_or_stracpy(format.select);
	fc->default_value = val;
	fc->default_state = has_attr(a, "selected");
	fc->mode = has_attr(a, "disabled") ? FORM_MODE_DISABLED : format.select_disabled;

	put_chrs(" ", 1, html_context.put_chars_f, html_context.part);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.style.attr |= AT_BOLD;
	put_chrs("[ ]", 3, html_context.put_chars_f, html_context.part);
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, html_context.put_chars_f, html_context.part);
	html_context.special_f(html_context.part, SP_CONTROL, fc);
}

static struct list_menu lnk_menu;

int
do_html_select(unsigned char *attr, unsigned char *html,
	       unsigned char *eof, unsigned char **end, struct part *part)
{
	struct conv_table *ct = html_context.special_f(part, SP_TABLE, NULL);
	struct form_control *fc;
	struct string lbl = NULL_STRING, orig_lbl = NULL_STRING;
	unsigned char **values = NULL;
	unsigned char **labels;
	unsigned char *t_name, *t_attr, *en;
	int t_namelen;
	int nnmi = 0;
	int order = 0;
	int preselect = -1;
	int group = 0;
	int i, max_width;

	if (has_attr(attr, "multiple")) return 1;
	html_focusable(attr);
	init_menu(&lnk_menu);

se:
        en = html;

see:
        html = en;
	while (html < eof && *html != '<') html++;

	if (html >= eof) {

abort:
		*end = html;
		if (lbl.source) done_string(&lbl);
		if (orig_lbl.source) done_string(&orig_lbl);
		if (values) {
			int j;

			for (j = 0; j < order; j++)
				mem_free_if(values[j]);
			mem_free(values);
		}
		destroy_menu(&lnk_menu);
		*end = en;
		return 0;
	}

	if (lbl.source) {
		unsigned char *q, *s = en;
		int l = html - en;

		while (l && isspace(s[0])) s++, l--;
		while (l && isspace(s[l-1])) l--;
		q = convert_string(ct, s, l, CSM_DEFAULT, NULL, NULL, NULL);
		if (q) add_to_string(&lbl, q), mem_free(q);
		add_bytes_to_string(&orig_lbl, s, l);
	}

	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}

	if (parse_element(html, eof, &t_name, &t_namelen, &t_attr, &en)) {
		html++;
		goto se;
	}

	if (!strlcasecmp(t_name, t_namelen, "/SELECT", 7)) {
		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);
		goto end_parse;
	}

	if (!strlcasecmp(t_name, t_namelen, "/OPTION", 7)) {
		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTION", 6)) {
		unsigned char *value, *label;

		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

		if (has_attr(t_attr, "disabled")) goto see;
		if (preselect == -1 && has_attr(t_attr, "selected")) preselect = order;
		value = get_attr_val(t_attr, "value");

		if (!mem_align_alloc(&values, order, order + 1, unsigned char *, 0xFF))
			goto abort;

		values[order++] = value;
		label = get_attr_val(t_attr, "label");
		if (label) new_menu_item(&lnk_menu, label, order - 1, 0);
		if (!value || !label) {
			init_string(&lbl);
			init_string(&orig_lbl);
			nnmi = !!label;
		}
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTGROUP", 8)
	    || !strlcasecmp(t_name, t_namelen, "/OPTGROUP", 9)) {
		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

		if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTGROUP", 8)) {
		unsigned char *label = get_attr_val(t_attr, "label");

		if (!label) {
			label = stracpy("");
			if (!label) goto see;
		}
		new_menu_item(&lnk_menu, label, -1, 0);
		group = 1;
	}
	goto see;


end_parse:
	*end = en;
	if (!order) goto abort;

	labels = mem_calloc(order, sizeof(unsigned char *));
	if (!labels) goto abort;

	fc = init_form_control(FC_SELECT, attr);
	if (!fc) {
		mem_free(labels);
		goto abort;
	}

	fc->name = get_attr_val(attr, "name");
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(values[fc->default_state]) : stracpy("");
	fc->nvalues = order;
	fc->values = values;
	fc->menu = detach_menu(&lnk_menu);
	fc->labels = labels;

	menu_labels(fc->menu, "", labels);
	put_chrs("[", 1, html_context.put_chars_f, part);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.style.attr |= AT_BOLD;

	max_width = 0;
	for (i = 0; i < order; i++) {
		if (!labels[i]) continue;
		int_lower_bound(&max_width, strlen(labels[i]));
	}

	for (i = 0; i < max_width; i++)
		put_chrs("_", 1, html_context.put_chars_f, part);

	kill_html_stack_item(&html_top);
	put_chrs("]", 1, html_context.put_chars_f, part);
	html_context.special_f(html_context.part, SP_CONTROL, fc);

	return 0;
}

void
html_textarea(unsigned char *a)
{
	INTERNAL("This should be never called");
}

void
do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof,
		 unsigned char **end, struct part *part)
{
	struct form_control *fc;
	unsigned char *p, *t_name, *wrap_attr;
	int t_namelen;
	int cols, rows;
	int i;

	html_focusable(attr);
	while (html < eof && (*html == '\n' || *html == '\r')) html++;
	p = html;
	while (p < eof && *p != '<') {

pp:
		p++;
	}
	if (p >= eof) {
		*end = eof;
		return;
	}
	if (parse_element(p, eof, &t_name, &t_namelen, NULL, end)) goto pp;
	if (strlcasecmp(t_name, t_namelen, "/TEXTAREA", 9)) goto pp;

	fc = init_form_control(FC_TEXTAREA, attr);
	if (!fc) return;

	fc->name = get_attr_val(attr, "name");
	fc->default_value = memacpy(html, p - html);
	for (p = fc->default_value; p && p[0]; p++) {
		/* FIXME: We don't cope well with entities here. Bugzilla uses
		 * &#13; inside of textarea and we fail miserably upon that
		 * one.  --pasky */
		if (p[0] == '\r') {
			if (p[1] == '\n'
			    || (p > fc->default_value && p[-1] == '\n')) {
				memcpy(p, p + 1, strlen(p));
				p--;
			} else {
				p[0] = '\n';
			}
		}
	}

	cols = get_num(attr, "cols");
	if (cols <= 0) cols = global_doc_opts->default_form_input_size;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > global_doc_opts->box.width)
		cols = global_doc_opts->box.width;
	fc->cols = cols;

	rows = get_num(attr, "rows");
	if (rows <= 0) rows = 1;
	if (rows > global_doc_opts->box.height)
		rows = global_doc_opts->box.height;
	fc->rows = rows;
	global_doc_opts->needs_height = 1;

	wrap_attr = get_attr_val(attr, "wrap");
	if (wrap_attr) {
		if (!strcasecmp(wrap_attr, "hard")
		    || !strcasecmp(wrap_attr, "physical")) {
			fc->wrap = FORM_WRAP_HARD;
		} else if (!strcasecmp(wrap_attr, "soft")
			   || !strcasecmp(wrap_attr, "virtual")) {
			fc->wrap = FORM_WRAP_SOFT;
		} else if (!strcasecmp(wrap_attr, "none")
			   || !strcasecmp(wrap_attr, "off")) {
			fc->wrap = FORM_WRAP_NONE;
		}
		mem_free(wrap_attr);

	} else if (has_attr(attr, "nowrap")) {
		fc->wrap = FORM_WRAP_NONE;

	} else {
		fc->wrap = FORM_WRAP_SOFT;
	}

	fc->maxlength = get_num(attr, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;

	if (rows > 1) ln_break(1, html_context.line_break_f, part);
	else put_chrs(" ", 1, html_context.put_chars_f, part);

	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.style.attr |= AT_BOLD;

	for (i = 0; i < rows; i++) {
		int j;

		for (j = 0; j < cols; j++)
			put_chrs("_", 1, html_context.put_chars_f, part);
		if (i < rows - 1)
			ln_break(1, html_context.line_break_f, part);
	}

	kill_html_stack_item(&html_top);
	if (rows > 1)
		ln_break(1, html_context.line_break_f, part);
	else
		put_chrs(" ", 1, html_context.put_chars_f, part);
	html_context.special_f(part, SP_CONTROL, fc);
}
