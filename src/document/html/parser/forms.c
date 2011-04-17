/* HTML forms parser */

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
html_form(struct html_context *html_context, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *al;
	struct form *form;

	html_context->was_br = 1;

	form = init_form();
	if (!form) return;

	form->method = FORM_METHOD_GET;
	form->form_num = a - html_context->startf;

	al = get_attr_val(a, "method", html_context->doc_cp);
	if (al) {
		if (!c_strcasecmp(al, "post")) {
			unsigned char *enctype;

			enctype  = get_attr_val(a, "enctype",
						html_context->doc_cp);

			form->method = FORM_METHOD_POST;
			if (enctype) {
				if (!c_strcasecmp(enctype, "multipart/form-data"))
					form->method = FORM_METHOD_POST_MP;
				else if (!c_strcasecmp(enctype, "text/plain"))
					form->method = FORM_METHOD_POST_TEXT_PLAIN;
				mem_free(enctype);
			}
		}
		mem_free(al);
	}
	form->onsubmit = get_attr_val(a, "onsubmit", html_context->doc_cp);
	al = get_attr_val(a, "name", html_context->doc_cp);
	if (al) form->name = al;

	al = get_attr_val(a, "action", html_context->doc_cp);
	/* The HTML specification at
	 * http://www.w3.org/TR/REC-html40/interact/forms.html#h-17.3 states
	 * that the behavior of an empty action attribute should be undefined.
	 * Mozilla handles action="" as action="<current-URI>" which seems
	 * reasonable. (bug 615) */
	if (al && *al) {
		form->action = join_urls(html_context->base_href, trim_chars(al, ' ', NULL));
		mem_free(al);

	} else {
		enum uri_component components = URI_ORIGINAL;

		mem_free_if(al);

		/* We have to do following for GET method, because we would end
		 * up with two '?' otherwise. */
		if (form->method == FORM_METHOD_GET)
			components = URI_FORM_GET;

		form->action = get_uri_string(html_context->base_href, components);

		/* No action URI should contain post data */
		assert(!form->action || !strchr(form->action, POST_CHAR));

		/* GET method URIs should not have '?'. */
		assert(!form->action
			|| form->method != FORM_METHOD_GET
			|| !strchr(form->action, '?'));
	}

	al = get_target(html_context->options, a);
	form->target = al ? al : stracpy(html_context->base_target);

	html_context->special_f(html_context, SP_FORM, form);
}


static int
get_form_mode(struct html_context *html_context, unsigned char *attr)
{
	if (has_attr(attr, "disabled", html_context->doc_cp))
		return FORM_MODE_DISABLED;

	if (has_attr(attr, "readonly", html_context->doc_cp))
		return FORM_MODE_READONLY;

	return FORM_MODE_NORMAL;
}

static struct form_control *
init_form_control(enum form_type type, unsigned char *attr,
                  struct html_context *html_context)
{
	struct form_control *fc;

	fc = mem_calloc(1, sizeof(*fc));
	if (!fc) return NULL;

	fc->type = type;
	fc->position = attr - html_context->startf;
	fc->mode = get_form_mode(html_context, attr);

	return fc;
}

void
html_button(struct html_context *html_context, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *al;
	struct form_control *fc;
	enum form_type type = FC_SUBMIT;
	int cp = html_context->doc_cp;

	html_focusable(html_context, a);

	al = get_attr_val(a, "type", cp);
	if (!al) goto no_type_attr;

	if (!c_strcasecmp(al, "button")) {
		type = FC_BUTTON;
	} else if (!c_strcasecmp(al, "reset")) {
		type = FC_RESET;
	} else if (c_strcasecmp(al, "submit")) {
		/* unknown type */
		mem_free(al);
		return;
	}
	mem_free(al);

no_type_attr:
	fc = init_form_control(type, a, html_context);
	if (!fc) return;

	fc->id = get_attr_val(a, "id", cp);
	fc->name = get_attr_val(a, "name", cp);
	fc->default_value = get_attr_val(a, "value", cp);
	if (!fc->default_value) {
		if (fc->type == FC_SUBMIT)
			fc->default_value = stracpy("Submit");
		else if (fc->type == FC_RESET)
			fc->default_value = stracpy("Reset");
		else if (fc->type == FC_BUTTON)
			fc->default_value = stracpy("Button");
	}
	if (!fc->default_value)
		fc->default_value = stracpy("");

	html_context->special_f(html_context, SP_CONTROL, fc);
	format.form = fc;
	format.style.attr |= AT_BOLD;
}

static void
html_input_format(struct html_context *html_context, unsigned char *a,
	   	  struct form_control *fc)
{
	put_chrs(html_context, " ", 1);
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	html_focusable(html_context, a);
	format.form = fc;
	if (format.title) mem_free(format.title);
	format.title = get_attr_val(a, "title", html_context->doc_cp);
	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
		{
			int i;

			format.style.attr |= AT_BOLD;
			for (i = 0; i < fc->size; i++)
				put_chrs(html_context, "_", 1);
			break;
		}
		case FC_CHECKBOX:
			format.style.attr |= AT_BOLD;
			put_chrs(html_context, "[&nbsp;]", 8);
			break;
		case FC_RADIO:
			format.style.attr |= AT_BOLD;
			put_chrs(html_context, "(&nbsp;)", 8);
			break;
		case FC_IMAGE:
		{
			unsigned char *al;

			mem_free_set(&format.image, NULL);
			al = get_url_val(a, "src", html_context->doc_cp);
			if (!al)
				al = get_url_val(a, "dynsrc",
				                 html_context->doc_cp);
			if (al) {
				format.image = join_urls(html_context->base_href, al);
				mem_free(al);
			}
			format.style.attr |= AT_BOLD;
			put_chrs(html_context, "[&nbsp;", 7);
			format.style.attr |= AT_NO_ENTITIES;
			if (fc->alt)
				put_chrs(html_context, fc->alt, strlen(fc->alt));
			else if (fc->name)
				put_chrs(html_context, fc->name, strlen(fc->name));
			else
				put_chrs(html_context, "Submit", 6);
			format.style.attr &= ~AT_NO_ENTITIES;

			put_chrs(html_context, "&nbsp;]", 7);
			break;
		}
		case FC_SUBMIT:
		case FC_RESET:
		case FC_BUTTON:
			format.style.attr |= AT_BOLD;
			put_chrs(html_context, "[&nbsp;", 7);
			if (fc->default_value) {
				format.style.attr |= AT_NO_ENTITIES;
				put_chrs(html_context, fc->default_value, strlen(fc->default_value));
				format.style.attr &= ~AT_NO_ENTITIES;
			}
			put_chrs(html_context, "&nbsp;]", 7);
			break;
		case FC_TEXTAREA:
		case FC_SELECT:
		case FC_HIDDEN:
			INTERNAL("bad control type");
	}
	pop_html_element(html_context);
	put_chrs(html_context, " ", 1);
}

void
html_input(struct html_context *html_context, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	unsigned char *al;
	struct form_control *fc;
	int cp = html_context->doc_cp;

	fc = init_form_control(FC_TEXT, a, html_context);
	if (!fc) return;

	al = get_attr_val(a, "type", cp);
	if (al) {
		if (!c_strcasecmp(al, "text")) fc->type = FC_TEXT;
		else if (!c_strcasecmp(al, "hidden")) fc->type = FC_HIDDEN;
		else if (!c_strcasecmp(al, "button")) fc->type = FC_BUTTON;
		else if (!c_strcasecmp(al, "checkbox")) fc->type = FC_CHECKBOX;
		else if (!c_strcasecmp(al, "radio")) fc->type = FC_RADIO;
		else if (!c_strcasecmp(al, "password")) fc->type = FC_PASSWORD;
		else if (!c_strcasecmp(al, "submit")) fc->type = FC_SUBMIT;
		else if (!c_strcasecmp(al, "reset")) fc->type = FC_RESET;
		else if (!c_strcasecmp(al, "file")) fc->type = FC_FILE;
		else if (!c_strcasecmp(al, "image")) fc->type = FC_IMAGE;
		/* else unknown type, let it default to FC_TEXT. */
		mem_free(al);
	}

	if (fc->type == FC_HIDDEN)
		fc->default_value = get_lit_attr_val(a, "value", cp);
	else if (fc->type != FC_FILE)
		fc->default_value = get_attr_val(a, "value", cp);
	if (!fc->default_value) {
		if (fc->type == FC_CHECKBOX)
			fc->default_value = stracpy("on");
		else if (fc->type == FC_SUBMIT)
			fc->default_value = stracpy("Submit");
		else if (fc->type == FC_RESET)
			fc->default_value = stracpy("Reset");
		else if (fc->type == FC_BUTTON)
			fc->default_value = stracpy("Button");
	}
	if (!fc->default_value)
		fc->default_value = stracpy("");

	fc->id = get_attr_val(a, "id", cp);
	fc->name = get_attr_val(a, "name", cp);

	fc->size = get_num(a, "size", cp);
	if (fc->size == -1)
		fc->size = html_context->options->default_form_input_size;
	fc->size++;
	if (fc->size > html_context->options->box.width)
		fc->size = html_context->options->box.width;
	fc->maxlength = get_num(a, "maxlength", cp);
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO)
		fc->default_state = has_attr(a, "checked", cp);
	if (fc->type == FC_IMAGE)
		fc->alt = get_attr_val(a, "alt", cp);

	if (fc->type != FC_HIDDEN) {
		html_input_format(html_context, a, fc);
	}

	html_context->special_f(html_context, SP_CONTROL, fc);
}

static struct list_menu lnk_menu;

static void
do_html_select(unsigned char *attr, unsigned char *html,
	       unsigned char *eof, unsigned char **end,
	       struct html_context *html_context)
{
	struct conv_table *ct = html_context->special_f(html_context, SP_TABLE, NULL);
	struct form_control *fc;
	struct string lbl = NULL_STRING, orig_lbl = NULL_STRING;
	unsigned char **values = NULL;
	unsigned char **labels;
	unsigned char *name, *t_attr, *en;
	int namelen;
	int nnmi = 0;
	int order = 0;
	int preselect = -1;
	int group = 0;
	int i, max_width;
	int closing_tag;

	html_focusable(html_context, attr);
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
		return;
	}

	if (lbl.source) {
		unsigned char *q, *s = en;
		int l = html - en;

		while (l && isspace(s[0])) s++, l--;
		while (l && isspace(s[l-1])) l--;
		q = convert_string(ct, s, l,
		                   html_context->options->cp,
		                   CSM_DEFAULT, NULL, NULL, NULL);
		if (q) add_to_string(&lbl, q), mem_free(q);
		add_bytes_to_string(&orig_lbl, s, l);
	}

	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}

	if (parse_element(html, eof, &name, &namelen, &t_attr, &en)) {
		html++;
		goto se;
	}

	if (!namelen) goto see;

	if (name[0] == '/') {
		namelen--;
		if (!namelen) goto see;
		name++;
		closing_tag = 1;
	} else {
		closing_tag = 0;
	}

	if (closing_tag && !c_strlcasecmp(name, namelen, "SELECT", 6)) {
		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);
		goto end_parse;
	}

	if (!c_strlcasecmp(name, namelen, "OPTION", 6)) {
		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

		if (!closing_tag) {
			unsigned char *value, *label;

			if (has_attr(t_attr, "disabled", html_context->doc_cp))
				goto see;
			if (preselect == -1
			    && has_attr(t_attr, "selected", html_context->doc_cp))
				preselect = order;
			value = get_attr_val(t_attr, "value", html_context->doc_cp);

			if (!mem_align_alloc(&values, order, order + 1, 0xFF))
				goto abort;

			values[order++] = value;
			label = get_attr_val(t_attr, "label", html_context->doc_cp);
			if (label) new_menu_item(&lnk_menu, label, order - 1, 0);
			if (!value || !label) {
				init_string(&lbl);
				init_string(&orig_lbl);
				nnmi = !!label;
			}
		}

		goto see;
	}

	if (!c_strlcasecmp(name, namelen, "OPTGROUP", 8)) {
		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

		if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;

		if (!closing_tag) {
			unsigned char *label;

			label = get_attr_val(t_attr, "label", html_context->doc_cp);

			if (!label) {
				label = stracpy("");
				if (!label) goto see;
			}
			new_menu_item(&lnk_menu, label, -1, 0);
			group = 1;
		}
	}

	goto see;


end_parse:
	*end = en;
	if (!order) goto abort;

	labels = mem_calloc(order, sizeof(unsigned char *));
	if (!labels) goto abort;

	fc = init_form_control(FC_SELECT, attr, html_context);
	if (!fc) {
		mem_free(labels);
		goto abort;
	}

	fc->id = get_attr_val(attr, "id", html_context->doc_cp);
	fc->name = get_attr_val(attr, "name", html_context->doc_cp);
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(values[fc->default_state]) : stracpy("");
	fc->nvalues = order;
	fc->values = values;
	fc->menu = detach_menu(&lnk_menu);
	fc->labels = labels;

	menu_labels(fc->menu, "", labels);
	put_chrs(html_context, "[", 1);
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	format.form = fc;
	format.style.attr |= AT_BOLD;

	max_width = 0;
	for (i = 0; i < order; i++) {
		if (!labels[i]) continue;
#ifdef CONFIG_UTF8
		if (html_context->options->utf8)
			int_lower_bound(&max_width,
					utf8_ptr2cells(labels[i], NULL));
		else
#endif /* CONFIG_UTF8 */
			int_lower_bound(&max_width, strlen(labels[i]));
	}

	for (i = 0; i < max_width; i++)
		put_chrs(html_context, "_", 1);

	pop_html_element(html_context);
	put_chrs(html_context, "]", 1);
	html_context->special_f(html_context, SP_CONTROL, fc);
}


static void
do_html_select_multiple(struct html_context *html_context, unsigned char *a,
                        unsigned char *html, unsigned char *eof,
                        unsigned char **end)
{
	unsigned char *al = get_attr_val(a, "name", html_context->doc_cp);

	if (!al) return;
	html_focusable(html_context, a);
	html_top->type = ELEMENT_DONT_KILL;
	mem_free_set(&format.select, al);
	format.select_disabled = has_attr(a, "disabled", html_context->doc_cp)
	                         ? FORM_MODE_DISABLED
	                         : FORM_MODE_NORMAL;
}

void
html_select(struct html_context *html_context, unsigned char *a,
            unsigned char *html, unsigned char *eof, unsigned char **end)
{
	if (has_attr(a, "multiple", html_context->doc_cp))
		do_html_select_multiple(html_context, a, html, eof, end);
	else
		do_html_select(a, html, eof, end, html_context);

}

void
html_option(struct html_context *html_context, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct form_control *fc;
	unsigned char *val;

	if (!format.select) return;

	val = get_attr_val(a, "value", html_context->doc_cp);
	if (!val) {
		struct string str;
		unsigned char *p, *r;
		unsigned char *name;
		int namelen;

		for (p = a - 1; *p != '<'; p--);

		if (!init_string(&str)) goto end_parse;
		if (parse_element(p, html_context->eoff, NULL, NULL, NULL, &p)) {
			INTERNAL("parse element failed");
			val = str.source;
			goto end_parse;
		}

se:
		while (p < html_context->eoff && isspace(*p)) p++;
		while (p < html_context->eoff && !isspace(*p) && *p != '<') {

sp:
			add_char_to_string(&str, *p ? *p : ' '), p++;
		}

		r = p;
		val = str.source; /* Has to be before the possible 'goto end_parse' */

		while (r < html_context->eoff && isspace(*r)) r++;
		if (r >= html_context->eoff) goto end_parse;
		if (r - 2 <= html_context->eoff && (r[1] == '!' || r[1] == '?')) {
			p = skip_comment(r, html_context->eoff);
			goto se;
		}
		if (parse_element(r, html_context->eoff, &name, &namelen, NULL, &p)) goto sp;

		if (namelen < 6) goto se;
		if (name[0] == '/') name++, namelen--;
		
		if (c_strlcasecmp(name, namelen, "OPTION", 6)
		    && c_strlcasecmp(name, namelen, "SELECT", 6)
		    && c_strlcasecmp(name, namelen, "OPTGROUP", 8))
			goto se;
	}

end_parse:
	fc = init_form_control(FC_CHECKBOX, a, html_context);
	if (!fc) {
		mem_free_if(val);
		return;
	}

	fc->id = get_attr_val(a, "id", html_context->doc_cp);
	fc->name = null_or_stracpy(format.select);
	fc->default_value = val;
	fc->default_state = has_attr(a, "selected", html_context->doc_cp);
	fc->mode = has_attr(a, "disabled", html_context->doc_cp)
	           ? FORM_MODE_DISABLED
	           : format.select_disabled;

	put_chrs(html_context, " ", 1);
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	format.form = fc;
	format.style.attr |= AT_BOLD;
	put_chrs(html_context, "[ ]", 3);
	pop_html_element(html_context);
	put_chrs(html_context, " ", 1);
	html_context->special_f(html_context, SP_CONTROL, fc);
}

void
html_textarea(struct html_context *html_context, unsigned char *attr,
              unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct form_control *fc;
	unsigned char *p, *t_name, *wrap_attr;
	int t_namelen;
	int cols, rows;
	int i;

	html_focusable(html_context, attr);
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
	if (c_strlcasecmp(t_name, t_namelen, "/TEXTAREA", 9)) goto pp;

	fc = init_form_control(FC_TEXTAREA, attr, html_context);
	if (!fc) return;

	fc->id = get_attr_val(attr, "id", html_context->doc_cp);
	fc->name = get_attr_val(attr, "name", html_context->doc_cp);
	fc->default_value = convert_string(NULL, html, p - html,
					   html_context->doc_cp,
					   CSM_DEFAULT, NULL, NULL, NULL);
	for (p = fc->default_value; p && p[0]; p++) {
		/* FIXME: We don't cope well with entities here. Bugzilla uses
		 * &#13; inside of textarea and we fail miserably upon that
		 * one.  --pasky */
		if (p[0] == '\r') {
			if (p[1] == '\n'
			    || (p > fc->default_value && p[-1] == '\n')) {
				memmove(p, p + 1, strlen(p));
				p--;
			} else {
				p[0] = '\n';
			}
		}
	}

	cols = get_num(attr, "cols", html_context->doc_cp);
	if (cols <= 0)
		cols = html_context->options->default_form_input_size;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > html_context->options->box.width)
		cols = html_context->options->box.width;
	fc->cols = cols;

	rows = get_num(attr, "rows", html_context->doc_cp);
	if (rows <= 0) rows = 1;
	if (rows > html_context->options->box.height)
		rows = html_context->options->box.height;
	fc->rows = rows;
	html_context->options->needs_height = 1;

	wrap_attr = get_attr_val(attr, "wrap", html_context->doc_cp);
	if (wrap_attr) {
		if (!c_strcasecmp(wrap_attr, "hard")
		    || !c_strcasecmp(wrap_attr, "physical")) {
			fc->wrap = FORM_WRAP_HARD;
		} else if (!c_strcasecmp(wrap_attr, "soft")
			   || !c_strcasecmp(wrap_attr, "virtual")) {
			fc->wrap = FORM_WRAP_SOFT;
		} else if (!c_strcasecmp(wrap_attr, "none")
			   || !c_strcasecmp(wrap_attr, "off")) {
			fc->wrap = FORM_WRAP_NONE;
		}
		mem_free(wrap_attr);

	} else if (has_attr(attr, "nowrap", html_context->doc_cp)) {
		fc->wrap = FORM_WRAP_NONE;

	} else {
		fc->wrap = FORM_WRAP_SOFT;
	}

	fc->maxlength = get_num(attr, "maxlength", html_context->doc_cp);
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;

	if (rows > 1) ln_break(html_context, 1);
	else put_chrs(html_context, " ", 1);

	html_stack_dup(html_context, ELEMENT_KILLABLE);
	format.form = fc;
	format.style.attr |= AT_BOLD;

	for (i = 0; i < rows; i++) {
		int j;

		for (j = 0; j < cols; j++)
			put_chrs(html_context, "_", 1);
		if (i < rows - 1)
			ln_break(html_context, 1);
	}

	pop_html_element(html_context);
	if (rows > 1)
		ln_break(html_context, 1);
	else
		put_chrs(html_context, " ", 1);
	html_context->special_f(html_context, SP_CONTROL, fc);
}
