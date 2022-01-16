/* General element handlers */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "bfu/listmenu.h"
#include "bookmarks/bookmarks.h"
#include "document/css/apply.h"
#include "document/html/frames.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "document/options.h"
#include "document/xml/tables.h"
#include "document/xml/tags.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/align.h"
#include "util/box.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"

#include <stdio.h>

#include <libxml++/libxml++.h>

static struct {
	int n;
	unsigned char *s;
} roman_tbl[] = {
	{1000,	"m"},
	{999,	"im"},
	{990,	"xm"},
	{900,	"cm"},
	{500,	"d"},
	{499,	"id"},
	{490,	"xd"},
	{400,	"cd"},
	{100,	"c"},
	{99,	"ic"},
	{90,	"xc"},
	{50,	"l"},
	{49,	"il"},
	{40,	"xl"},
	{10,	"x"},
	{9,	"ix"},
	{5,	"v"},
	{4,	"iv"},
	{1,	"i"},
	{0,	NULL}
};

enum hlink_type {
	LT_UNKNOWN = 0,
	LT_START,
	LT_PARENT,
	LT_NEXT,
	LT_PREV,
	LT_CONTENTS,
	LT_INDEX,
	LT_GLOSSARY,
	LT_CHAPTER,
	LT_SECTION,
	LT_SUBSECTION,
	LT_APPENDIX,
	LT_HELP,
	LT_SEARCH,
	LT_BOOKMARK,
	LT_COPYRIGHT,
	LT_AUTHOR,
	LT_ICON,
	LT_ALTERNATE,
	LT_ALTERNATE_LANG,
	LT_ALTERNATE_MEDIA,
	LT_ALTERNATE_STYLESHEET,
	LT_STYLESHEET,
};

enum hlink_direction {
	LD_UNKNOWN = 0,
	LD_REV,
	LD_REL,
};

struct hlink {
	enum hlink_type type;
	enum hlink_direction direction;
	unsigned char *content_type;
	unsigned char *media;
	unsigned char *href;
	unsigned char *hreflang;
	unsigned char *title;
	unsigned char *lang;
	unsigned char *name;
/* Not implemented yet.
	unsigned char *charset;
	unsigned char *target;
	unsigned char *id;
	unsigned char *class_;
	unsigned char *dir;
*/
};

struct lt_default_name {
	enum hlink_type type;
	unsigned char *str;
};

/* TODO: i18n */
/* XXX: Keep the (really really ;) default name first */
static struct lt_default_name lt_names[] = {
	{ LT_START, "start" },
	{ LT_START, "top" },
	{ LT_START, "home" },
	{ LT_PARENT, "parent" },
	{ LT_PARENT, "up" },
	{ LT_NEXT, "next" },
	{ LT_PREV, "previous" },
	{ LT_PREV, "prev" },
	{ LT_CONTENTS, "contents" },
	{ LT_CONTENTS, "toc" },
	{ LT_INDEX, "index" },
	{ LT_GLOSSARY, "glossary" },
	{ LT_CHAPTER, "chapter" },
	{ LT_SECTION, "section" },
	{ LT_SUBSECTION, "subsection" },
	{ LT_SUBSECTION, "child" },
	{ LT_SUBSECTION, "sibling" },
	{ LT_APPENDIX, "appendix" },
	{ LT_HELP, "help" },
	{ LT_SEARCH, "search" },
	{ LT_BOOKMARK, "bookmark" },
	{ LT_ALTERNATE_LANG, "alt. language" },
	{ LT_ALTERNATE_MEDIA, "alt. media" },
	{ LT_ALTERNATE_STYLESHEET, "alt. stylesheet" },
	{ LT_STYLESHEET, "stylesheet" },
	{ LT_ALTERNATE, "alternate" },
	{ LT_COPYRIGHT, "copyright" },
	{ LT_AUTHOR, "author" },
	{ LT_AUTHOR, "made" },
	{ LT_AUTHOR, "owner" },
	{ LT_ICON, "icon" },
	{ LT_UNKNOWN, NULL }
};


static void
roman(struct string  *p, unsigned n)
{
	int i = 0;

	if (n >= 4000) {
		add_to_string(p, "---");
		return;
	}
	if (!n) {
		add_to_string(p, "o");
		return;
	}
	while (n) {
		while (roman_tbl[i].n <= n) {
			n -= roman_tbl[i].n;
			add_to_string(p, roman_tbl[i].s);
		}
		i++;
		assertm(!(n && !roman_tbl[i].n),
			"BUG in roman number converter");
		if_assert_failed break;
	}
}

static int
tags_get_form_mode(struct html_context *html_context, void *node)
{
	xmlpp::Element *el = node;
	xmlpp::Attribute *attr;

	attr = el->get_attribute("disabled");
	if (attr) {
		return FORM_MODE_DISABLED;
	}

	attr = el->get_attribute("readonly");
	if (attr) {
		return FORM_MODE_READONLY;
	}

	return FORM_MODE_NORMAL;
}


static struct el_form_control *
tags_init_form_control(enum form_type type, void *node,
                  struct html_context *html_context)
{
	struct el_form_control *fc;

	fc = (struct el_form_control *)mem_calloc(1, sizeof(*fc));
	if (!fc) return NULL;

	fc->type = type;
	fc->position = ++html_context->ff;
//	fc->position = attr - html_context->startf;
	fc->mode = tags_get_form_mode(html_context, node);

	return fc;
}

#if 0
static void
html_apply_canvas_bgcolor(struct source_renderer *rendererer)
{
	struct html_context *html_context = rendererer->html_context;

#ifdef CONFIG_CSS
	/* If there are any CSS twaks regarding bgcolor, make sure we will get
	 * it _and_ prefer it over bgcolor attribute. */
	if (html_context->options->css_enable)
		css_apply(html_context, html_top, &html_context->css_styles,
		          &html_context->stack);
#endif

	if (par_elformat.color.background != elformat.style.color.background) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_bottom;

		html_context->was_body_background = 1;
		e->parattr.color.background = e->attr.style.color.background = par_elformat.color.background = elformat.style.color.background;
	}

	if (html_context->has_link_lines
	    && par_elformat.color.background != html_context->options->default_style.color.background
	    && !search_html_stack(html_context, DOM_HTML_ELEMENT_TYPE_BODY)) {
		html_context->special_f(html_context, SP_COLOR_LINK_LINES);
	}
}
#endif

static void tags_html_linebrk(struct source_renderer *renderer, unsigned char *al);
static void tags_html_h(int h, void *node, unsigned char *a,
       enum format_align default_align, struct source_renderer *renderer,
       unsigned char *html, unsigned char *eof, unsigned char **end);


static void
tags_set_fragment_identifier(struct html_context *html_context,
                        unsigned char *id_attr)
{
	if (id_attr) {
		html_context->special_f(html_context, SP_TAG, id_attr);
		mem_free(id_attr);
	}
}

#if 0
static void
tags_add_fragment_identifier(struct html_context *html_context,
                        struct part *part, unsigned char *attr)
{
	struct part *saved_part = html_context->part;

	html_context->part = part;
	html_context->special_f(html_context, SP_TAG, attr);
	html_context->part = saved_part;
}
#endif

void
tags_html_focusable(struct source_renderer *renderer, void *node)
{
	struct html_context *html_context = renderer->html_context;
	int32_t tabindex;

	elformat.accesskey = 0;
	elformat.tabindex = 0x80000000;

	xmlpp::Element *el = node;
	xmlpp::ustring accesskey_value = el->get_attribute_value("accesskey");

	if (accesskey_value != "") {
		elformat.accesskey = accesskey_string_to_unicode(accesskey_value.c_str());
	}

	xmlpp::ustring tabindex_value = el->get_attribute_value("tabindex");
	if (tabindex_value != "") {
		tabindex = atoi(tabindex_value.c_str());

		if (0 < tabindex && tabindex < 32767) {
			elformat.tabindex = (tabindex & 0x7fff) << 16;
		}
	}

	xmlpp::ustring string_value = el->get_attribute_value("onclick");
	char *value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.onclick, value);

	string_value = el->get_attribute_value("ondblclick");
	value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.ondblclick, value);

	string_value = el->get_attribute_value("onmouseover");
	value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.onmouseover, value);

	string_value = el->get_attribute_value("onhover");
	value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.onhover, value);

	string_value = el->get_attribute_value("onfocus");
	value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.onfocus, value);

	string_value = el->get_attribute_value("onmouseout");
	value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.onmouseout, value);

	string_value = el->get_attribute_value("onblur");
	value = NULL;
	if (string_value != "") {
		value = memacpy(string_value.c_str(), string_value.size());
	}
	mem_free_set(&elformat.onblur, value);
}

void
tags_html_a(struct source_renderer *renderer, void *node, unsigned char *a,
       unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *href;

	xmlpp::Element *anchor = node;
	xmlpp::ustring href_value = anchor->get_attribute_value("href");

	if (href_value == "") {
		return;
	}

	href = memacpy(href_value.c_str(), href_value.size());

	if (href) {
		unsigned char *target = NULL;
		unsigned char *title = NULL;

		mem_free_set(&elformat.link,
			     join_urls(html_context->base_href,
				       trim_chars(href, ' ', 0)));

		mem_free(href);

		xmlpp::ustring target_value = anchor->get_attribute_value("target");

		if (target_value != "") {
			target = memacpy(target_value.c_str(), target_value.size());
		}

		if (target) {
			mem_free_set(&elformat.target, target);
		} else {
			mem_free_set(&elformat.target, stracpy(html_context->base_target));
		}

		if (0) {
			; /* Shut up compiler */
#ifdef CONFIG_GLOBHIST
		} else if (get_global_history_item(elformat.link)) {
			elformat.style.color.foreground = elformat.color.vlink;
			html_top->pseudo_class &= ~ELEMENT_LINK;
			html_top->pseudo_class |= ELEMENT_VISITED;
#endif
#ifdef CONFIG_BOOKMARKS
		} else if (get_bookmark(elformat.link)) {
			elformat.style.color.foreground = elformat.color.bookmark_link;
			html_top->pseudo_class &= ~ELEMENT_VISITED;
			/* XXX: Really set ELEMENT_LINK? --pasky */
			html_top->pseudo_class |= ELEMENT_LINK;
#endif
		} else {
			elformat.style.color.foreground = elformat.color.clink;
			html_top->pseudo_class &= ~ELEMENT_VISITED;
			html_top->pseudo_class |= ELEMENT_LINK;
		}

		xmlpp::ustring title_value = anchor->get_attribute_value("title");

		if (title_value != "") {
			title = memacpy(title_value.c_str(), title_value.size());
		}

		mem_free_set(&elformat.title, title);

		tags_html_focusable(renderer, (void *)anchor);

	} else {
		pop_html_element(html_context);
	}

	xmlpp::ustring name_value = anchor->get_attribute_value("name");

	if (name_value != "") {
		unsigned char *name = memacpy(name_value.c_str(), name_value.size());

		tags_set_fragment_identifier(html_context, name);
	}
}

void
tags_html_a_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_abbr(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_abbr_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_address(struct source_renderer *renderer, void *node, unsigned char *a,
             unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	par_elformat.leftmargin++;
	par_elformat.align = ALIGN_LEFT;
}

void
tags_html_address_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_applet(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_applet_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_area(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_area_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_article(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_article_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_aside(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_aside_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_audio(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_audio_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_b(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	elformat.style.attr |= AT_BOLD;
}

void
tags_html_b_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_base(struct source_renderer *renderer, void *no, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *element = no;
	unsigned char *al;

	xmlpp::ustring href_value = element->get_attribute_value("href");

	if (href_value != "") {
		al = memacpy(href_value.c_str(), href_value.size());

		if (al) {
			unsigned char *base = join_urls(html_context->base_href, al);
			struct uri *uri = base ? get_uri(base, URI_NONE) : NULL;

			mem_free(al);
			mem_free_if(base);

			if (uri) {
				done_uri(html_context->base_href);
				html_context->base_href = uri;
			}
		}
	}
	xmlpp::ustring target_value = element->get_attribute_value("target");

	if (target_value != "") {
		al = memacpy(target_value.c_str(), target_value.size());

		if (al) mem_free_set(&html_context->base_target, al);
	}
}

void
tags_html_base_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_basefont(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_basefont_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdi(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdi_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdo(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdo_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_blockquote(struct source_renderer *renderer, void *node, unsigned char *a,
                unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;

	par_elformat.align = ALIGN_LEFT;
	if (par_elformat.blockquote_level == 0) {
		par_elformat.orig_leftmargin = par_elformat.leftmargin;
		par_elformat.blockquote_level++;
	}
	par_elformat.blockquote_level++;
}

void
tags_html_blockquote_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;

	if (par_elformat.blockquote_level == 2) par_elformat.blockquote_level--;
	if (par_elformat.blockquote_level > 0) par_elformat.blockquote_level--;
}

void
tags_html_apply_canvas_bgcolor(struct source_renderer *renderer)
{
	struct html_context *html_context = renderer->html_context;
#ifdef CONFIG_CSS
	/* If there are any CSS twaks regarding bgcolor, make sure we will get
	 * it _and_ prefer it over bgcolor attribute. */
	if (html_context->options->css_enable)
		css_apply(html_context, html_top, &html_context->css_styles,
		          &html_context->stack);
#endif

	if (par_elformat.color.background != elformat.style.color.background) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_bottom;

		html_context->was_body_background = 1;
		e->parattr.color.background = e->attr.style.color.background = par_elformat.color.background = elformat.style.color.background;
	}

	if (html_context->has_link_lines
	    && par_elformat.color.background != html_context->options->default_style.color.background
	    && !search_html_stack(html_context, "body")) {
		html_context->special_f(html_context, SP_COLOR_LINK_LINES);
	}
}

void
tags_html_body(struct source_renderer *renderer, void *no, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;

	xmlpp::ustring text_value = node->get_attribute_value("text");
	get_color2(html_context, text_value.c_str(), &elformat.style.color.foreground);

	xmlpp::ustring link_value = node->get_attribute_value("link");
	get_color2(html_context, link_value.c_str(), &elformat.color.clink);

	xmlpp::ustring vlink_value = node->get_attribute_value("vlink");
	get_color2(html_context, vlink_value.c_str(), &elformat.color.vlink);

	xmlpp::ustring bgcolor_value = node->get_attribute_value("bgcolor");
	int v = get_color2(html_context, bgcolor_value.c_str(), &elformat.style.color.background);

	if (-1 != v) {
		html_context->was_body_background = 1;
	}

	html_context->was_body = 1; /* this will be used by "meta inside body" */
	tags_html_apply_canvas_bgcolor(renderer);
}

void
tags_html_body_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_br(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	//html_linebrk(html_context, a, html, eof, end);
	if (html_context->was_br)
		ln_break(html_context, 2);
	else
		html_context->was_br = 1;
}

void
tags_html_br_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_button(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *al = NULL;
	struct el_form_control *fc;
	enum form_type type = FC_SUBMIT;
	xmlpp::Element *button = node;

	tags_html_focusable(renderer, node);

	xmlpp::ustring type_value = button->get_attribute_value("type");
	if (type_value != "")  {
		al = memacpy(type_value.c_str(), type_value.size());
	}
	
//	al = get_attr_val(a, "type", cp);
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
	fc = tags_init_form_control(type, node, html_context);
	if (!fc) return;

	xmlpp::ustring disabled = button->get_attribute_value("disabled");

	if (disabled == "true" || disabled == "1" || disabled == "disabled") {
		fc->mode = FORM_MODE_DISABLED;
	}

	xmlpp::ustring id_value = button->get_attribute_value("id");
	if (id_value != "") {
		fc->id = memacpy(id_value.c_str(), id_value.size());
	}
	
	//fc->id = get_attr_val(a, "id", cp);
	xmlpp::ustring name_value = button->get_attribute_value("name");
	if (name_value != "") {
		fc->name = memacpy(name_value.c_str(), name_value.size());
	}
	//fc->name = get_attr_val(a, "name", cp);

	xmlpp::ustring value_value = button->get_attribute_value("value");
	if (true) {
		fc->default_value = memacpy(value_value.c_str(), value_value.size());
	}
	//fc->default_value = get_attr_val(a, "value", cp);
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
	elformat.form = fc;
	elformat.style.attr |= AT_BOLD;
}

void
tags_html_button_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_canvas(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_canvas_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_caption(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_caption_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_center(struct source_renderer *renderer, void *node, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	par_elformat.align = ALIGN_CENTER;
	if (!html_context->table_level)
		par_elformat.leftmargin = par_elformat.rightmargin = 0;
}

void
tags_html_center_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_cite(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_cite_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_code(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_code_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_col(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_col_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_colgroup(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_colgroup_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_data(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_data_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_datalist(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_datalist_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dd(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	kill_html_stack_until(html_context, 0, "", "DL", NULL);

	par_elformat.leftmargin = par_elformat.dd_margin + 3;

	if (!html_context->table_level) {
		par_elformat.leftmargin += 5;
		int_upper_bound(&par_elformat.leftmargin, par_elformat.width / 2);
	}
	par_elformat.align = ALIGN_LEFT;
}

void
tags_html_dd_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_del(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_del_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_details(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_details_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dfn(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dfn_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dialog(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dialog_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dir(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dir_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_div(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_div_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dl(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	xmlpp::ustring compact = node->get_attribute_value("compact");

	par_elformat.flags &= ~P_COMPACT;

	if (compact != "") {
		par_elformat.flags |= P_COMPACT;
	}

	if (par_elformat.list_level) par_elformat.leftmargin += 5;
	par_elformat.list_level++;
	par_elformat.list_number = 0;
	par_elformat.align = ALIGN_LEFT;
	par_elformat.dd_margin = par_elformat.leftmargin;
	html_top->type = ELEMENT_DONT_KILL;

	if (!(par_elformat.flags & P_COMPACT)) {
		ln_break(html_context, 2);
		html_top->linebreak = 2;
	}
}

void
tags_html_dl_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dt(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	xmlpp::ustring compact = node->get_attribute_value("compact");

	kill_html_stack_until(html_context, 0, "", "DL", NULL);
	par_elformat.align = ALIGN_LEFT;
	par_elformat.leftmargin = par_elformat.dd_margin;

	if (!(par_elformat.flags & P_COMPACT) && (compact == ""))
		ln_break(html_context, 2);
}

void
tags_html_dt_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_em(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_em_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_embed(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_embed_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_fieldset(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_fieldset_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figcaption(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figcaption_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figure(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figure_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_font(struct source_renderer *renderer, void *no, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	xmlpp::ustring size_value = node->get_attribute_value("size");
	if (size_value != "") {
		unsigned char *al = memacpy(size_value.c_str(), size_value.size());

		if (al) {
			int p = 0;
			unsigned s;
			unsigned char *nn = al;
			unsigned char *end;

			if (*al == '+') p = 1, nn++;
			else if (*al == '-') p = -1, nn++;

			errno = 0;
			s = strtoul(nn, (char **) &end, 10);

			if (!errno && *nn && !*end) {
				if (s > 7) s = 7;
				if (!p) elformat.fontsize = s;
				else elformat.fontsize += p * s;
				if (elformat.fontsize < 1) elformat.fontsize = 1;
				else if (elformat.fontsize > 7) elformat.fontsize = 7;
			}
			mem_free(al);
		}
	}
	xmlpp::ustring color_value = node->get_attribute_value("color");
	get_color2(html_context, color_value.c_str(), &elformat.style.color.foreground);
}

void
tags_html_font_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_footer(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_footer_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_form(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *al=NULL;
	struct form *form;
	xmlpp::Element *form_node = node;
	
	html_context->was_br = 1;

	form = init_form();
	if (!form) return;

	form->method = FORM_METHOD_GET;
	form->form_num = ++html_context->ff;
//	form->form_num = a - html_context->startf;

	xmlpp::ustring method_value = form_node->get_attribute_value("method");
	if (method_value != "") {
		al = memacpy(method_value.c_str(), method_value.size());
	}
	
	//al = get_attr_val(a, "method", html_context->doc_cp);
	if (al) {
		if (!c_strcasecmp(al, "post")) {
			xmlpp::ustring enctype_value = form_node->get_attribute_value("enctype");
			if (enctype_value != "") {
				unsigned char *enctype = memacpy(enctype_value.c_str(), enctype_value.size());
//			enctype  = get_attr_val(a, "enctype",
//						html_context->doc_cp);

				form->method = FORM_METHOD_POST;
				if (enctype) {
					if (!c_strcasecmp(enctype, "multipart/form-data"))
						form->method = FORM_METHOD_POST_MP;
					else if (!c_strcasecmp(enctype, "text/plain"))
						form->method = FORM_METHOD_POST_TEXT_PLAIN;
					mem_free(enctype);
				}
			}
		}
		mem_free(al);
	}
	xmlpp::ustring onsubmit_value = form_node->get_attribute_value("onsubmit");
	if (onsubmit_value != "") {
		form->onsubmit = memacpy(onsubmit_value.c_str(), onsubmit_value.size());
	}

	xmlpp::ustring name_value = form_node->get_attribute_value("name");
	//form->onsubmit = get_attr_val(a, "onsubmit", html_context->doc_cp);
	if (name_value != "") {
		form->name = memacpy(name_value.c_str(), name_value.size());
	}
	//al = get_attr_val(a, "name", html_context->doc_cp);
	//if (al) form->name = al;

	xmlpp::ustring action_value = form_node->get_attribute_value("action");
	if (action_value != "") {
		al = memacpy(action_value.c_str(), action_value.size());
	
//	al = get_attr_val(a, "action", html_context->doc_cp);
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
			if (form->method == FORM_METHOD_GET) {
				components = URI_FORM_GET;
			}

			form->action = get_uri_string(html_context->base_href, components);

			/* No action URI should contain post data */
			assert(!form->action || !strchr((const char *)form->action, POST_CHAR));

			/* GET method URIs should not have '?'. */
			assert(!form->action
			|| form->method != FORM_METHOD_GET
			|| !strchr((const char *)form->action, '?'));
		}
	}
	al = NULL;
	xmlpp::ustring target_value = form_node->get_attribute_value("target");
	if (target_value != "") {
		al = memacpy(target_value.c_str(), target_value.size());
	}
	//al = get_target(html_context->options, a);
	form->target = al ? al : stracpy(html_context->base_target);

	html_context->special_f(html_context, SP_FORM, form);
}

void
tags_html_form_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_frame(struct source_renderer *renderer, void *no, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	xmlpp::ustring src_value = node->get_attribute_value("src");
	unsigned char *src = NULL, *name = NULL, *url;

	if (src_value != "") {
		src = memacpy(src_value.c_str(), src_value.size());
	}

	if (!src) {
		url = stracpy("about:blank");
	} else {
		url = join_urls(html_context->base_href, src);
		mem_free(src);
	}
	if (!url) return;

	xmlpp::ustring name_value = node->get_attribute_value("name");
	if (name_value != "") {
		name = memacpy(name_value.c_str(), name_value.size());
	}

	if (!name) {
		name = stracpy(url);
	} else if (!name[0]) {
		/* When name doesn't have a value */
		mem_free(name);
		name = stracpy(url);
	}
	if (!name) {
		mem_free_if(url);
		return;
	}

	if (!html_context->options->frames || !html_top->frameset) {
		tags_html_focusable(renderer, no);
		put_link_line("Frame: ", name, url, "", html_context);

	} else {
		if (html_context->special_f(html_context, SP_USED, NULL)) {
			html_context->special_f(html_context, SP_FRAME,
					       html_top->frameset, name, url);
		}
	}

	mem_free(name);
	mem_free(url);
}

void
tags_html_frame_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_frameset(struct source_renderer *renderer, void *no, unsigned char *a,
              unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	struct frameset_param fp;
	unsigned char *cols = NULL, *rows = NULL;
	int width, height;

	/* XXX: This is still not 100% correct. We should also ignore the
	 * frameset when we encountered anything 3v1l (read as: non-whitespace
	 * text/element/anything) in the document outside of <head>. Well, this
	 * is still better than nothing and it should heal up the security
	 * concerns at least because sane sites should enclose the documents in
	 * <body> elements ;-). See also bug 171. --pasky */
	if (search_html_stack(html_context, "body")
	    || !html_context->options->frames
	    || !html_context->special_f(html_context, SP_USED, NULL))
		return;

	xmlpp::ustring cols_value = node->get_attribute_value("cols");
	if (cols_value != "") {
		cols = memacpy(cols_value.c_str(), cols_value.size());
	}
	
	if (!cols) {
		cols = stracpy("100%");
		if (!cols) return;
	}

	xmlpp::ustring rows_value = node->get_attribute_value("rows");
	if (rows_value != "") {
		rows = memacpy(rows_value.c_str(), rows_value.size());
	}

	if (!rows) {
		rows = stracpy("100%");
	       	if (!rows) {
			mem_free(cols);
			return;
		}
	}

	if (!html_top->frameset) {
		width = html_context->options->document_width;
		height = html_context->options->box.height;
		html_context->options->needs_height = 1;
	} else {
		struct frameset_desc *frameset_desc = html_top->frameset;
		int offset;

		if (frameset_desc->box.y >= frameset_desc->box.height)
			goto free_and_return;
		offset = frameset_desc->box.x
			 + frameset_desc->box.y * frameset_desc->box.width;
		width = frameset_desc->frame_desc[offset].width;
		height = frameset_desc->frame_desc[offset].height;
	}

	fp.width = fp.height = NULL;

	parse_frame_widths(cols, width, HTML_FRAME_CHAR_WIDTH,
			   &fp.width, &fp.x);
	parse_frame_widths(rows, height, HTML_FRAME_CHAR_HEIGHT,
			   &fp.height, &fp.y);

	fp.parent = html_top->frameset;
	if (fp.x && fp.y) {
		html_top->frameset = html_context->special_f(html_context, SP_FRAMESET, &fp);
	}
	mem_free_if(fp.width);
	mem_free_if(fp.height);

free_and_return:
	mem_free(cols);
	mem_free(rows);
}



void
tags_html_frameset_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h1(struct source_renderer *renderer, void *node, unsigned char *a,
	unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	elformat.style.attr |= AT_BOLD;
	tags_html_h(1, node, a, ALIGN_CENTER, renderer, html, eof, end);
}

void
tags_html_h1_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h2(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(2, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h2_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h3(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(3, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h3_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h4(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(4, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h4_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h5(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(5, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h5_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h6(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(6, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h6_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_head(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	/* This makes sure it gets to the stack and helps tame down unclosed
	 * <title>. */
	renderer->html_context->skip_html = 1;
}

void
tags_html_head_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	renderer->html_context->skip_html = 0;
}

void
tags_html_header(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_header_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_hgroup(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_hgroup_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_hr(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	int i = -1/* = par_elformat.width - 10*/;
	unsigned char r = (unsigned char) BORDER_DHLINE;
	xmlpp::Element *node = no;
	unsigned char *al = NULL;
	int q = -1;
	//dom_long q = 0;

	xmlpp::ustring size_value = node->get_attribute_value("size");
	if (size_value != "") {
		al = memacpy(size_value.c_str(), size_value.size());
		q = get_num2(al);
	}

	if (q >= 0 && q < 2) r = (unsigned char) BORDER_SHLINE;
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	par_elformat.align = ALIGN_CENTER;
	mem_free_set(&elformat.link, NULL);
	elformat.form = NULL;

	xmlpp::ustring align_value = node->get_attribute_value("align");
	if (align_value != "") {
		al = memacpy(align_value.c_str(), align_value.size());
		tags_html_linebrk(renderer, al);
	}
	if (par_elformat.align == ALIGN_JUSTIFY) par_elformat.align = ALIGN_CENTER;
	par_elformat.leftmargin = par_elformat.rightmargin = html_context->margin;

	xmlpp::ustring width_value = node->get_attribute_value("width");
	if (width_value != "") {
		al = memacpy(width_value.c_str(), width_value.size());
		i = get_width2(al, 1, html_context);
	}

	if (i == -1) i = get_html_max_width();
	elformat.style.attr = AT_GRAPHICS;
	html_context->special_f(html_context, SP_NOWRAP, 1);
	while (i-- > 0) {
		put_chrs(html_context, &r, 1);
	}
	html_context->special_f(html_context, SP_NOWRAP, 0);
	ln_break(html_context, 2);
	pop_html_element(html_context);
}

void
tags_html_hr_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_html(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/* This is here just to get CSS stuff applied. */

	/* Modify the root HTML element - format_html_part() will take
	 * this from there. */
	struct html_element *e = html_bottom;

	if (par_elformat.color.background != elformat.style.color.background)
		e->parattr.color.background = e->attr.style.color.background = par_elformat.color.background = elformat.style.color.background;
}

void
tags_html_html_close(struct source_renderer *renderer, void *node, unsigned char *a,
                unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;

	if (html_top->type >= ELEMENT_KILLABLE
	    && !html_context->was_body_background) {
		tags_html_apply_canvas_bgcolor(renderer);
	}
}

void
tags_html_i(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	elformat.style.attr |= AT_ITALIC;
}

void
tags_html_i_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_iframe(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_iframe_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}
/* Returns an allocated string made after @label
 * but limited to @max_len length, by truncating
 * the middle of @label string, which is replaced
 * by an asterisk ('*').
 * If @max_len < 0 it returns NULL.
 * If @max_len == 0 it returns an unmodified copy
 * of @label string.
 * In either case, it may return NULL if a memory
 * allocation failure occurs.
 * Example:
 * truncate_label("some_string", 5) => "so*ng" */
static unsigned char *
truncate_label(unsigned char *label, int max_len)
{
	unsigned char *new_label;
	int len = strlen(label);
	int left_part_len;
	int right_part_len;

	if (max_len < 0) return NULL;
	if (max_len == 0 || len <= max_len)
		return stracpy(label);

	right_part_len = left_part_len = max_len / 2;

	if (left_part_len + right_part_len + 1 > max_len)
		right_part_len--;

	new_label = (unsigned char *)mem_alloc(max_len + 1);
	if (!new_label) return NULL;

	if (left_part_len)
		memcpy(new_label, label, left_part_len);

	new_label[left_part_len] = '*';

	if (right_part_len)
		memcpy(new_label + left_part_len + 1,
		       label + len - right_part_len, right_part_len);

	new_label[max_len] = '\0';

	return new_label;
}

/* Get image filename from its src attribute. */
static unsigned char *
get_image_filename_from_src(int max_len, unsigned char *src)
{
	unsigned char *text = NULL;
	unsigned char *start, *filename;
	int len;

	if (!src) return NULL;
	/* We can display image as [foo.gif]. */

	len = strcspn(src, "?");

	for (start = src + len; start > src; start--)
		if (dir_sep(start[-1])) {
			break;
		}

	len -= start - src;

	filename = memacpy(start, len);
	if (filename) {
		/* XXX: Due to a compatibility alias (added: 2004-12-15 in
		 * 0.10pre3.CVS for document.browse.images.file_tags) this can
		 * return a negative @max_len. */
		text = truncate_label(filename, max_len);
		mem_free(filename);
	}

	return text;
}


/* Returns an allocated string containing formatted @label. */
static unsigned char *
get_image_label(int max_len, unsigned char *label)
{
	unsigned char *formatted_label;

	if (!label) return NULL;

	formatted_label = truncate_label(label, max_len);
	mem_free(label);

	return formatted_label;
}

static void
put_image_label(struct source_renderer *renderer, void *node, unsigned char *label)
{
	struct html_context *html_context = renderer->html_context;
	color_T saved_foreground;
	enum text_style_format saved_attr;

	/* This is not 100% appropriate for <img>, but well, accepting
	 * accesskey and tabindex near <img> is just our little
	 * extension to the standard. After all, it makes sense. */
	tags_html_focusable(renderer, node);

	saved_foreground = elformat.style.color.foreground;
	saved_attr = elformat.style.attr;
	elformat.style.color.foreground = elformat.color.image_link;
	elformat.style.attr |= AT_NO_ENTITIES;
	put_chrs(html_context, label, strlen(label));
	elformat.style.color.foreground = saved_foreground;
	elformat.style.attr = saved_attr;
}

static void
tags_html_img_do(struct source_renderer *renderer, void *node, unsigned char *a, unsigned char *object_src)
{
	struct html_context *html_context = renderer->html_context;
	int ismap, usemap = 0;
	bool ismap_b = 0;
	int add_brackets = 0;
	unsigned char *src = NULL;
	unsigned char *label = NULL;
	unsigned char *usemap_attr;
	struct document_options *options = html_context->options;
	int display_style = options->image_link.display_style;

	xmlpp::Element *img_element = node;

	/* Note about display_style:
	 * 0     means always display IMG
	 * 1     means always display filename
	 * 2     means display alt/title attribute if possible, IMG if not
	 * 3     means display alt/title attribute if possible, filename if not */

	xmlpp::ustring usemap_value = img_element->get_attribute_value("usemap");
	if (usemap_value != "") {
		usemap_attr = memacpy(usemap_value.c_str(), usemap_value.size());
	} else {
		usemap_attr = NULL;
	}

	//usemap_attr = get_attr_val(a, "usemap", html_context->doc_cp);
	if (usemap_attr) {
		unsigned char *joined_urls = join_urls(html_context->base_href,
						       usemap_attr);
		unsigned char *map_url;

		mem_free(usemap_attr);
		if (!joined_urls) return;
		map_url = straconcat("MAP@", joined_urls,
				     (unsigned char *) NULL);
		mem_free(joined_urls);
		if (!map_url) return;

		html_stack_dup(html_context, ELEMENT_KILLABLE);
		mem_free_set(&elformat.link, map_url);
		elformat.form = NULL;
		elformat.style.attr |= AT_BOLD;
		usemap = 1;
	}

	xmlpp::ustring ismap_value = img_element->get_attribute_value("ismap");

	ismap = elformat.link && (ismap_value != "") && !usemap;
//	ismap = elformat.link
//	        && has_attr(a, "ismap", html_context->doc_cp)
//	        && !usemap;

	if (display_style == 2 || display_style == 3) {
		xmlpp::ustring alt_value = img_element->get_attribute_value("alt");
		if (alt_value != "") {
			label = memacpy(alt_value.c_str(), alt_value.size());
		}

		//label = get_attr_val(a, "alt", html_context->doc_cp);
		if (!label) {
			xmlpp::ustring title_value = img_element->get_attribute_value("title");
			if (title_value != "")  {
				label = memacpy(title_value.c_str(), title_value.size());
			}

			//label = get_attr_val(a, "title", html_context->doc_cp);
		}

		/* Little hack to preserve rendering of [   ], in directory listings,
		 * but we still want to drop extra spaces in alt or title attribute
		 * to limit display width on certain websites. --Zas */
		if (label && strlen(label) > 5) clr_spaces(label);
	}

	src = null_or_stracpy(object_src);
	if (!src) {
		xmlpp::ustring src_value = img_element->get_attribute_value("src");
		if (src_value != "") {
			src = memacpy(src_value.c_str(), src_value.size());
		}
		//src = get_url_val(a, "src", html_context->doc_cp);
	}
//	if (!src) src = get_url_val(a, "dynsrc", html_context->doc_cp);

	/* If we have no label yet (no title or alt), so
	 * just use default ones, or image filename. */
	if (!label || !*label) {
		mem_free_set(&label, NULL);
		/* Do we want to display images with no alt/title and with no
		 * link on them ?
		 * If not, just exit now. */
		if (!options->images && !elformat.link) {
			mem_free_if(src);
			if (usemap) pop_html_element(html_context);
			return;
		}

		add_brackets = 1;

		if (usemap) {
			label = stracpy("USEMAP");
		} else if (ismap) {
			label = stracpy("ISMAP");
		} else {
			if (display_style == 3)
				label = get_image_filename_from_src(options->image_link.filename_maxlen, src);
		}

	} else {
		label = get_image_label(options->image_link.label_maxlen, label);
	}

	if (!label || !*label) {
		mem_free_set(&label, NULL);
		add_brackets = 1;
		if (display_style == 1)
			label = get_image_filename_from_src(options->image_link.filename_maxlen, src);
		if (!label || !*label)
			mem_free_set(&label, stracpy("IMG"));
	}

	mem_free_set(&elformat.image, NULL);
	mem_free_set(&elformat.title, NULL);

	if (label) {
		int img_link_tag = options->image_link.tagging;

		if (img_link_tag && (img_link_tag == 2 || add_brackets)) {
			unsigned char *img_link_prefix = options->image_link.prefix;
			unsigned char *img_link_suffix = options->image_link.suffix;
			unsigned char *new_label = straconcat(img_link_prefix, label, img_link_suffix, (unsigned char *) NULL);

			if (new_label) mem_free_set(&label, new_label);
		}

		if (!options->image_link.show_any_as_links) {
			put_image_label(renderer, node, label);

		} else {
			if (src) {
				elformat.image = join_urls(html_context->base_href, src);
			}

			xmlpp::ustring title_value = img_element->get_attribute_value("title");
			if (title_value != "") {
				elformat.title = memacpy(title_value.c_str(), title_value.size());
			}
			//elformat.title = get_attr_val(a, "title", html_context->doc_cp);

			if (ismap) {
				unsigned char *new_link;

				html_stack_dup(html_context, ELEMENT_KILLABLE);
				new_link = straconcat(elformat.link, "?0,0", (unsigned char *) NULL);
				if (new_link) {
					mem_free_set(&elformat.link, new_link);
				}
			}

			put_image_label(renderer, node, label);

			if (ismap) pop_html_element(html_context);
			mem_free_set(&elformat.image, NULL);
			mem_free_set(&elformat.title, NULL);
		}

		mem_free(label);
	}

	mem_free_if(src);
	if (usemap) pop_html_element(html_context);
}

void
tags_html_img(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	tags_html_img_do(renderer, node, a, NULL);
}

void
tags_html_img_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}


static void
tags_html_input_format(struct source_renderer *renderer, void *node, unsigned char *a,
	   	  struct el_form_control *fc)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, " ", 1);
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	tags_html_focusable(renderer, node);
	elformat.form = fc;
	mem_free_if(elformat.title);
	xmlpp::Element *input = node;

	xmlpp::ustring title_value = input->get_attribute_value("title");
	if (title_value != "") {
		elformat.title = memacpy(title_value.c_str(), title_value.size());
	}
	
	//elformat.title = get_attr_val(a, "title", html_context->doc_cp);
	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
		{
			int i;

			elformat.style.attr |= AT_BOLD;
			for (i = 0; i < fc->size; i++)
				put_chrs(html_context, "_", 1);
			break;
		}
		case FC_CHECKBOX:
			elformat.style.attr |= AT_BOLD;
			put_chrs(html_context, "[&nbsp;]", 8);
			break;
		case FC_RADIO:
			elformat.style.attr |= AT_BOLD;
			put_chrs(html_context, "(&nbsp;)", 8);
			break;
		case FC_IMAGE:
		{
			unsigned char *al = NULL;

			mem_free_set(&elformat.image, NULL);

			xmlpp::ustring src_value = input->get_attribute_value("src");
			if (src_value != "") {
				al = memacpy(src_value.c_str(), src_value.size());
			}

			//al = get_url_val(a, "src", html_context->doc_cp);
#if 0
			if (!al) {
				al = get_url_val(a, "dynsrc",
				                 html_context->doc_cp);
			}
#endif
			if (al) {
				elformat.image = join_urls(html_context->base_href, al);
				mem_free(al);
			}
			elformat.style.attr |= AT_BOLD;
			put_chrs(html_context, "[&nbsp;", 7);
			elformat.style.attr |= AT_NO_ENTITIES;
			if (fc->alt)
				put_chrs(html_context, fc->alt, strlen(fc->alt));
			else if (fc->name)
				put_chrs(html_context, fc->name, strlen(fc->name));
			else
				put_chrs(html_context, "Submit", 6);
			elformat.style.attr &= ~AT_NO_ENTITIES;

			put_chrs(html_context, "&nbsp;]", 7);
			break;
		}
		case FC_SUBMIT:
		case FC_RESET:
		case FC_BUTTON:
			elformat.style.attr |= AT_BOLD;
			put_chrs(html_context, "[&nbsp;", 7);
			if (fc->default_value) {
				elformat.style.attr |= AT_NO_ENTITIES;
				put_chrs(html_context, fc->default_value, strlen(fc->default_value));
				elformat.style.attr &= ~AT_NO_ENTITIES;
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
tags_html_input(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *al = NULL;
	struct el_form_control *fc;

	xmlpp::Element *input = node;
	unsigned int size = 0;
	
	fc = tags_init_form_control(FC_TEXT, node, html_context);
	if (!fc) return;

	xmlpp::ustring disabled = input->get_attribute_value("disabled");
	if (disabled == "disabled" || disabled == "true" || disabled == "1") {
		fc->mode = FORM_MODE_DISABLED;
	}

	if (disabled == "") {
		xmlpp::ustring readonly = input->get_attribute_value("readonly");
		if (readonly == "readonly" || readonly == "true" || readonly == "1") {
			fc->mode = FORM_MODE_READONLY;
		}
	}

	xmlpp::ustring type_value = input->get_attribute_value("type");
	if (type_value != "") {
		al = memacpy(type_value.c_str(), type_value.size());
	}
	
//	al = get_attr_val(a, "type", cp);
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

	xmlpp::ustring value_value = input->get_attribute_value("value");
	if (true) {
		if (fc->type == FC_HIDDEN) {
			fc->default_value = memacpy(value_value.c_str(), value_value.size());
		} else if (fc->type != FC_FILE) {
			fc->default_value = memacpy(value_value.c_str(), value_value.size());
		}
	}
	
//	if (fc->type == FC_HIDDEN)
//		fc->default_value = get_lit_attr_val(a, "value", cp);
//	else if (fc->type != FC_FILE)
//		fc->default_value = get_attr_val(a, "value", cp);
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

	xmlpp::ustring id_value = input->get_attribute_value("id");
	if (id_value != "") {
		fc->id = memacpy(id_value.c_str(), id_value.size());
	}
	
	//fc->id = get_attr_val(a, "id", cp);

	xmlpp::ustring name_value = input->get_attribute_value("name");
	if (name_value != "") {
		fc->name = memacpy(name_value.c_str(), name_value.size());
	}
	//fc->name = get_attr_val(a, "name", cp);

	xmlpp::ustring size_value = input->get_attribute_value("size");
	if (size_value != "") {
		fc->size = atoi(size_value.c_str());
	}
	//fc->size = get_num(a, "size", cp);
	if (fc->size <= 0)
		fc->size = html_context->options->default_form_input_size;
	fc->size++;
	if (fc->size > html_context->options->document_width)
		fc->size = html_context->options->document_width;

	xmlpp::ustring maxlength_value = input->get_attribute_value("maxlength");

	if (maxlength_value != "") {
		fc->maxlength = atoi(maxlength_value.c_str());
	}

	//fc->maxlength = get_num(a, "maxlength", cp);
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) {
		xmlpp::ustring checked_value = input->get_attribute_value("checked");
		bool checked = (checked_value == "checked" || checked_value == "true" || checked_value == "1");
		//fc->default_state = has_attr(a, "checked", cp);
		fc->default_state = checked;
	}
	if (fc->type == FC_IMAGE) {
		xmlpp::ustring alt_value = input->get_attribute_value("alt");
		if (alt_value != "") {
			fc->alt = memacpy(alt_value.c_str(), alt_value.size());
		}
//		fc->alt = get_attr_val(a, "alt", cp);
	}

	if (fc->type != FC_HIDDEN) {
		tags_html_input_format(renderer, node, a, fc);
	}

	html_context->special_f(html_context, SP_CONTROL, fc);
}

void
tags_html_input_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ins(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ins_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_isindex(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_isindex_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_kbd(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_kbd_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_keygen(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_keygen_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_label(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_label_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_legend(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_legend_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_li(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	int t = par_elformat.flags & P_LISTMASK;

	/* When handling the code <li><li> @was_li will be 1 and it means we
	 * have to insert a line break since no list item content has done it
	 * for us. */
	if (html_context->was_li) {
		html_context->line_breax = 0;
		ln_break(html_context, 1);
	}

	/*kill_html_stack_until(html_context, 0
	                      "", "UL", "OL", NULL);*/
	if (t == P_NO_BULLET) {
		/* Print nothing. */
	} else if (!par_elformat.list_number) {
		if (t == P_O) /* Print U+25E6 WHITE BULLET. */
			put_chrs(html_context, "&#9702;", 7);
		else if (t == P_SQUARE) /* Print U+25AA BLACK SMALL SQUARE. */
			put_chrs(html_context, "&#9642;", 7);
		else /* Print U+2022 BULLET. */
			put_chrs(html_context, "&#8226;", 7);
		put_chrs(html_context, "&nbsp;", 6);
		par_elformat.leftmargin += 2;
		par_elformat.align = ALIGN_LEFT;

	} else {
		long s = -1;
		unsigned char c = 0;
		int nlen;
		int t = par_elformat.flags & P_LISTMASK;
		struct string n;

		xmlpp::ustring s_value = node->get_attribute_value("value");
		if (s_value != "") {
			s = atol(s_value.c_str());
		}

		if (!init_string(&n)) return;

		if (s != -1) par_elformat.list_number = s;

		if (t == P_ALPHA || t == P_alpha) {
			unsigned char n0;

			put_chrs(html_context, "&nbsp;", 6);
			c = 1;
			n0 = par_elformat.list_number
			       ? (par_elformat.list_number - 1) % 26
			         + (t == P_ALPHA ? 'A' : 'a')
			       : 0;
			if (n0) add_char_to_string(&n, n0);

		} else if (t == P_ROMAN || t == P_roman) {
			roman(&n, par_elformat.list_number);
			if (t == P_ROMAN) {
				unsigned char *x;

				for (x = n.source; *x; x++) *x = c_toupper(*x);
			}

		} else {
			unsigned char n0[64];
			if (par_elformat.list_number < 10) {
				put_chrs(html_context, "&nbsp;", 6);
				c = 1;
			}

			ulongcat(n0, NULL, par_elformat.list_number, (sizeof(n) - 1), 0);
			add_to_string(&n, n0);
		}

		nlen = n.length;
		put_chrs(html_context, n.source, nlen);
		put_chrs(html_context, ".&nbsp;", 7);
		par_elformat.leftmargin += nlen + c + 2;
		par_elformat.align = ALIGN_LEFT;
		done_string(&n);

		{
			struct html_element *element;

			element = search_html_stack(html_context, "ol");
			if (element)
				element->parattr.list_number = par_elformat.list_number + 1;
		}

		par_elformat.list_number = 0;
	}

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = 2;
	html_context->was_li = 1;
}

void
tags_html_li_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

/* Search for default name for this link according to its type. */
static unsigned char *
get_lt_default_name(struct hlink *link)
{
	struct lt_default_name *entry = lt_names;

	assert(link);

	while (entry && entry->str) {
		if (entry->type == link->type) return entry->str;
		entry++;
	}

	return "unknown";
}

static void
tags_html_link_clear(struct hlink *link)
{
	assert(link);

	mem_free_if(link->content_type);
	mem_free_if(link->media);
	mem_free_if(link->href);
	mem_free_if(link->hreflang);
	mem_free_if(link->title);
	mem_free_if(link->lang);
	mem_free_if(link->name);

	memset(link, 0, sizeof(*link));
}

/* Parse a link and return results in @link.
 * It tries to identify known types. */
static int
tags_html_link_parse(struct source_renderer *renderer, void *node, unsigned char *a,
                struct hlink *link)
{
	//struct html_context *html_context = renderer->html_context;
	xmlpp::Element *link_element = node;

	int i;

	assert(/*a &&*/ link);
	memset(link, 0, sizeof(*link));

	xmlpp::ustring href_value = link_element->get_attribute_value("href");
	if (href_value != "") {
		link->href = memacpy(href_value.c_str(), href_value.size());
	}
	
//	link->href = get_url_val(a, "href", html_context->doc_cp);
	if (!link->href) return 0;

	xmlpp::ustring lang_value = link_element->get_attribute_value("lang");
	if (lang_value != "") {
		link->lang = memacpy(lang_value.c_str(), lang_value.size());
	}
	//link->lang = get_attr_val(a, "lang", html_context->doc_cp);

	xmlpp::ustring hreflang_value = link_element->get_attribute_value("hreflang");
	if (hreflang_value != "") {
		link->hreflang = memacpy(hreflang_value.c_str(), hreflang_value.size());
	}

//	link->hreflang = get_attr_val(a, "hreflang", html_context->doc_cp);
	xmlpp::ustring title_value = link_element->get_attribute_value("title");
	if (title_value != "") {
		link->title = memacpy(title_value.c_str(), title_value.size());
	}

//	link->title = get_attr_val(a, "title", html_context->doc_cp);
	xmlpp::ustring type_value = link_element->get_attribute_value("type");
	if (type_value != "") {
		link->content_type = memacpy(type_value.c_str(), type_value.size());
	}
	//link->content_type = get_attr_val(a, "type", html_context->doc_cp);

	xmlpp::ustring media_value = link_element->get_attribute_value("media");
	if (media_value != "") {
		link->media = memacpy(media_value.c_str(), media_value.size());
	}
	//link->media = get_attr_val(a, "media", html_context->doc_cp);

	xmlpp::ustring rel_value = link_element->get_attribute_value("rel");
	if (rel_value != "") {
		link->name = memacpy(rel_value.c_str(), rel_value.size());
	}
	//link->name = get_attr_val(a, "rel", html_context->doc_cp);
	if (link->name) {
		link->direction = LD_REL;
	} else {
		xmlpp::ustring rev_value = link_element->get_attribute_value("rev");
		if (rev_value != "") {
			link->name = memacpy(rev_value.c_str(), rev_value.size());
		}
		//link->name = get_attr_val(a, "rev", html_context->doc_cp);
		if (link->name) link->direction = LD_REV;
	}

	if (!link->name) return 1;

	/* TODO: fastfind */
	for (i = 0; lt_names[i].str; i++)
		if (!c_strcasecmp(link->name, lt_names[i].str)) {
			link->type = lt_names[i].type;
			return 1;
		}

	if (c_strcasestr((const char *)link->name, "icon") ||
	   (link->content_type && c_strcasestr((const char *)link->content_type, "icon"))) {
		link->type = LT_ICON;

	} else if (c_strcasestr((const char *)link->name, "alternate")) {
		link->type = LT_ALTERNATE;
		if (link->lang)
			link->type = LT_ALTERNATE_LANG;
		else if (c_strcasestr((const char *)link->name, "stylesheet") ||
			 (link->content_type && c_strcasestr((const char *)link->content_type, "css")))
			link->type = LT_ALTERNATE_STYLESHEET;
		else if (link->media)
			link->type = LT_ALTERNATE_MEDIA;

	} else if (link->content_type && c_strcasestr((const char *)link->content_type, "css")) {
		link->type = LT_STYLESHEET;
	}

	return 1;
}

void
tags_html_link(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	int link_display = html_context->options->meta_link_display;
	unsigned char *name;
	struct hlink link;
	struct string text;
	int name_neq_title = 0;
	int first = 1;

#ifndef CONFIG_CSS
	if (!link_display) return;
#endif
	if (!tags_html_link_parse(renderer, node, a, &link)) return;
	if (!link.href) goto free_and_return;

#ifdef CONFIG_CSS
	if (link.type == LT_STYLESHEET
	    && supports_html_media_attr(link.media)) {
		int len = strlen(link.href);

		import_css_stylesheet(&html_context->css_styles,
				      html_context->base_href, link.href, len);
	}

	if (!link_display) goto free_and_return;
#endif

	/* Ignore few annoying links.. */
	if (link_display < 5 &&
	    (link.type == LT_ICON ||
	     link.type == LT_AUTHOR ||
	     link.type == LT_STYLESHEET ||
	     link.type == LT_ALTERNATE_STYLESHEET)) goto free_and_return;

	if (!link.name || link.type != LT_UNKNOWN)
		/* Give preference to our default names for known types. */
		name = get_lt_default_name(&link);
	else
		name = link.name;

	if (!name) goto free_and_return;
	if (!init_string(&text)) goto free_and_return;

	tags_html_focusable(renderer, node);

	if (link.title) {
		add_to_string(&text, link.title);
		name_neq_title = strcmp(link.title, name);
	} else
		add_to_string(&text, name);

	if (link_display == 1) goto put_link_line;	/* Only title */

#define APPEND(what) do { \
		add_to_string(&text, first ? " (" : ", "); \
		add_to_string(&text, (what)); \
		first = 0; \
	} while (0)

	if (name_neq_title) {
		APPEND(name);
	}

	if (link_display >= 3 && link.hreflang) {
		APPEND(link.hreflang);
	}

	if (link_display >= 4 && link.content_type) {
		APPEND(link.content_type);
	}

	if (link.lang && link.type == LT_ALTERNATE_LANG &&
	    (link_display < 3 || (link.hreflang &&
				  c_strcasecmp(link.hreflang, link.lang)))) {
		APPEND(link.lang);
	}

	if (link.media) {
		APPEND(link.media);
	}

#undef APPEND

	if (!first) add_char_to_string(&text, ')');

put_link_line:
	{
		unsigned char *prefix = (link.direction == LD_REL)
					? "Link: " : "Reverse link: ";
		unsigned char *link_name = (text.length)
					   ? text.source : name;

		put_link_line(prefix, link_name, link.href,
			      html_context->base_target, html_context);

		if (text.source) done_string(&text);
	}

free_and_return:
	tags_html_link_clear(&link);
}

void
tags_html_link_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_main(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_main_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_map(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_map_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_mark(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_mark_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menu(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menu_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menuitem(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menuitem_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_meta(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	/* html_handle_body_meta() does all the work. */
}

void
tags_html_meta_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_meter(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_meter_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_nav(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_nav_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

//void
//tags_html_noframes(struct source_renderer *renderer, void *node, unsigned char *a,
//              unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
//{
//	struct html_element *element;
//
//	if (!html_context->options->frames) return;
//
//	element = search_html_stack(html_context, "frameset");
//	if (element && !element->frameset) return;
//
//	html_skip(html_context, a);
//}

void
tags_html_noscript(struct source_renderer *renderer, void *node, unsigned char *a,
              unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/* We shouldn't throw <noscript> away until our ECMAScript support is
	 * halfway decent. */
#ifdef CONFIG_ECMASCRIPT
	if (get_opt_bool("ecmascript.enable", NULL)
            && get_opt_bool("ecmascript.ignore_noscript", NULL))
			html_context->skip_html = 1;
		//html_skip(html_context, a);
#endif
}

void
tags_html_noscript_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/* We shouldn't throw <noscript> away until our ECMAScript support is
	 * halfway decent. */
#ifdef CONFIG_ECMASCRIPT
	if (get_opt_bool("ecmascript.enable", NULL)
            && get_opt_bool("ecmascript.ignore_noscript", NULL))
			html_context->skip_html = 0;
		//html_skip(html_context, a);
#endif
	
}

void
tags_html_object(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_object_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ol(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	unsigned char *al;
	long st = 1;

	par_elformat.list_level++;
	xmlpp::ustring start_value = node->get_attribute_value("start");
	if (start_value != "") {
		st = atol(start_value.c_str());
	}
	if (st == -1) st = 1;
	par_elformat.list_number = st;
	par_elformat.flags = P_NUMBER;

	xmlpp::ustring type_value = node->get_attribute_value("type");
	if (type_value != "") {
		al = memacpy(type_value.c_str(), type_value.size());

		if (al) {
			if (*al && !al[1]) {
				if (*al == '1') par_elformat.flags = P_NUMBER;
				else if (*al == 'a') par_elformat.flags = P_alpha;
				else if (*al == 'A') par_elformat.flags = P_ALPHA;
				else if (*al == 'r') par_elformat.flags = P_roman;
				else if (*al == 'R') par_elformat.flags = P_ROMAN;
				else if (*al == 'i') par_elformat.flags = P_roman;
				else if (*al == 'I') par_elformat.flags = P_ROMAN;
			}
			mem_free(al);
		}
	}

	par_elformat.leftmargin += (par_elformat.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_elformat.leftmargin, par_elformat.width / 2);

	par_elformat.align = ALIGN_LEFT;
	html_top->type = ELEMENT_DONT_KILL;
}

void
tags_html_ol_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_optgroup(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_optgroup_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_option(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_option_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_output(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_output_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_p(struct source_renderer *renderer, void *node, unsigned char *a,
       unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *element = node;
	int_lower_bound(&par_elformat.leftmargin, html_context->margin);
	int_lower_bound(&par_elformat.rightmargin, html_context->margin);
	/*par_elformat.align = ALIGN_LEFT;*/

	xmlpp::ustring align_value = element->get_attribute_value("align");
	if (align_value != "") {
		unsigned char *al = memacpy(align_value.c_str(), align_value.size());

		tags_html_linebrk(renderer, al);
	}
}

void
tags_html_p_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_param(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_param_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_picture(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_picture_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_pre(struct source_renderer *renderer, void *node, unsigned char *a,
         unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	elformat.style.attr |= AT_PREFORMATTED;
	par_elformat.leftmargin = (par_elformat.leftmargin > 1);
	par_elformat.rightmargin = 0;
}

void
tags_html_pre_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_progress(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_progress_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

/* TODO: Add more languages.
 * Entities can be used in these strings.  */
static unsigned char *quote_char[2] = { "\"", "'" };

void
tags_html_q(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/* An HTML document containing extremely many repetitions of
	 * "<q>" could cause @html_context->quote_level to overflow.
	 * Because it is unsigned, it then wraps around to zero, and
	 * we don't get a negative array index here.  If the document
	 * then tries to close the quotes with "</q>", @html_quote_close
	 * won't let the quote level wrap back, so it will render the
	 * quotes incorrectly, but such a document probably doesn't
	 * make sense anyway.  */
	unsigned char *q = quote_char[html_context->quote_level++ % 2];

	put_chrs(html_context, q, 1);
}

void
tags_html_q_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *q;

	if (html_context->quote_level > 0)
		html_context->quote_level--;

	q = quote_char[html_context->quote_level % 2];

	put_chrs(html_context, q, 1);
}

void
tags_html_rp(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_rp_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_rt(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_rt_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ruby(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ruby_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_s(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_s_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_samp(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_samp_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_script(struct source_renderer *renderer, void *no, unsigned char *a,
            unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
#ifdef CONFIG_ECMASCRIPT
	/* TODO: <noscript> processing. Well, same considerations apply as to
	 * CSS property display: none processing. */
	/* TODO: Charsets for external scripts. */
	unsigned char *type_ = NULL, *src = NULL;
	//int in_comment = 0;
#endif
	//html_context->skip_html = 1;
	html_skip(html_context, a);

#ifdef CONFIG_ECMASCRIPT
	/* We try to process nested <script> if we didn't process the parent
	 * one. That's why's all the fuzz. */
	/* Ref:
	 * http://www.ietf.org/internet-drafts/draft-hoehrmann-script-types-03.txt
	 */
	xmlpp::ustring type_value = node->get_attribute_value("type");
	if (type_value != "") {
		type_ = memacpy(type_value.c_str(), type_value.size());
	}
	
	if (type_) {
		unsigned char *pos = type_;

		if (!c_strncasecmp(type_, "text/", 5)) {
			pos += 5;

		} else if (!c_strncasecmp(type_, "application/", 12)) {
			pos += 12;

		} else {
			mem_free(type_);
not_processed:
			/* Permit nested scripts and retreat. */
			html_top->invisible++;
			return;
		}

		if (!c_strncasecmp(pos, "javascript", 10)) {
			int len = strlen(pos);

			if (len > 10 && !isdigit(pos[10])) {
				mem_free(type_);
				goto not_processed;
			}

		} else if (c_strcasecmp(pos, "ecmascript")
		    && c_strcasecmp(pos, "jscript")
		    && c_strcasecmp(pos, "livescript")
		    && c_strcasecmp(pos, "x-javascript")
		    && c_strcasecmp(pos, "x-ecmascript")) {
			mem_free(type_);
			goto not_processed;
		}

		mem_free(type_);
	}

	if (html_context->part->document) {
		xmlpp::ustring src_value = node->get_attribute_value("src");

		if (src_value != "") {
			src = memacpy(src_value.c_str(), src_value.size());
		}

		if (src) {
			/* External reference. */

			unsigned char *import_url;
			struct uri *uri;

			if (!get_opt_bool("ecmascript.enable", NULL)) {
				mem_free(src);
				goto not_processed;
			}

			/* HTML <head> urls should already be fine but we can.t detect them. */
			import_url = join_urls(html_context->base_href, src);
			mem_free(src);
			if (!import_url) goto imported;

			uri = get_uri(import_url, URI_BASE);
			if (!uri) goto imported;

			/* Request the imported script as part of the document ... */
			html_context->special_f(html_context, SP_SCRIPT, uri);
			done_uri(uri);

			/* Create URL reference onload snippet. */
			insert_in_string(&import_url, 0, "^", 1);
			add_to_string_list(&html_context->part->document->onload_snippets,
		                       import_url, -1);

imported:
			/* Retreat. Do not permit nested scripts, tho'. */
			if (import_url) mem_free(import_url);
			return;
		}
	}

	auto children = node->get_children();
	auto it = children.begin();
	auto en = children.end();

	for (; it != en; ++it) {
		xmlpp::CdataNode *cdata = dynamic_cast<xmlpp::CdataNode*>(*it);

		if (cdata) {
			xmlpp::ustring content = cdata->get_content();

			if (html_context->part->document) {
				add_to_string_list(&html_context->part->document->onload_snippets, content.c_str(), content.size());
			}
		}
	}

#if 0
	/* Positive, grab the rest and interpret it. */

	/* First position to the real script start. */
	while (html < eof && *html <= ' ') html++;
	if (eof - html > 4 && !strncmp(html, "<!--", 4)) {
		in_comment = 1;
		/* We either skip to the end of line or to -->. */
		for (; *html != '\n' && *html != '\r' && eof - html >= 3; html++) {
			if (!strncmp(html, "-->", 3)) {
				/* This means the document is probably broken.
				 * We will now try to process the rest of
				 * <script> contents, which is however likely
				 * to be empty. Should we try to process the
				 * comment too? Currently it seems safer but
				 * less tolerant to broken pages, if there are
				 * any like this. */
				html += 3;
				in_comment = 0;
				break;
			}
		}
	}

	*end = html;

	/* Now look ahead for the script end. The <script> contents is raw
	 * CDATA, so we just look for the ending tag and need not care for
	 * any quote marks counting etc - YET, we are more tolerant and permit
	 * </script> stuff inside of the script if the whole <script> element
	 * contents is wrapped in a comment. See i.e. Mozilla bug 26857 for fun
	 * reading regarding this. */
	for (; *end < eof; (*end)++) {
		unsigned char *name;
		int namelen;

		if (in_comment) {
			/* TODO: If we ever get some standards-quirk mode
			 * distinction, this should be disabled in the
			 * standards mode (and we should just look for CDATA
			 * end, which is "</"). --pasky */
			if (eof - *end >= 3 && !strncmp(*end, "-->", 3)) {
				/* Next iteration will jump passed the ending '>' */
				(*end) += 2;
				in_comment = 0;
			}
			continue;
			/* XXX: Scan for another comment? That's admittelly
			 * already stretching things a little bit to an
			 * extreme ;-). */
		}

		if (**end != '<')
			continue;
		/* We want to land before the closing element, that's why we
		 * don't pass @end also as the appropriate parse_element()
		 * argument. */
		if (parse_element(*end, eof, &name, &namelen, NULL, NULL))
			continue;
		if (c_strlcasecmp(name, namelen, "/script", 7))
			continue;
		/* We have won! */
		break;
	}
	if (*end >= eof) {
		/* Either the document is not completely loaded yet or it's
		 * broken. At any rate, run away screaming. */
		*end = eof; /* Just for sanity. */
		return;
	}

	if (html_context->part->document && *html != '^') {
		add_to_string_list(&html_context->part->document->onload_snippets,
		                   html, *end - html);
	}
#endif
#endif
}

void
tags_html_script_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	renderer->html_context->skip_html = 0;
}

void
tags_html_section(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_section_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

static void
do_tags_html_select_multiple(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *element = node;

	xmlpp::ustring name_value = element->get_attribute_value("name");
	if (name_value != "") {
		bool disabled = false;
		unsigned char *al = memacpy(name_value.c_str(), name_value.size());

		if (!al) return;
	
	//unsigned char *al = get_attr_val(a, "name", html_context->doc_cp);

	//if (!al) return;
		tags_html_focusable(renderer, node);
		html_top->type = ELEMENT_DONT_KILL;
		mem_free_set(&elformat.select, al);

		xmlpp::ustring disabled_value = element->get_attribute_value("disabled");
		if (disabled_value != "") {
			disabled = (disabled_value == "disabled" || disabled_value == "true" || disabled_value == "1");
		}
		elformat.select_disabled = disabled ? FORM_MODE_DISABLED : FORM_MODE_NORMAL;
	//elformat.select_disabled = has_attr(a, "disabled", html_context->doc_cp)
	//                         ? FORM_MODE_DISABLED
	//                         : FORM_MODE_NORMAL;
	}
}

static void
do_tags_html_select(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	//struct conv_table *ct = html_context->special_f(html_context, SP_TABLE, NULL);
	struct el_form_control *fc;
	struct string lbl = NULL_STRING, orig_lbl = NULL_STRING;
	unsigned char **values = NULL;
	unsigned char **labels;
	//unsigned char *name, *t_attr, *en;
	//int namelen;
	int nnmi = 0;
	int order = 0;
	int preselect = -1;
	int group = 0;
	int i, max_width;

	tags_html_focusable(renderer, node);
	init_menu(&lnk_menu);

	xmlpp::Element *select = node;
	xmlpp::Node::NodeList options = select->get_children();

	if (options.size()) {
		int len = 0, i;
		len = options.size();
		order = 0;
		auto it = options.begin();
		auto end = options.end();
		for (i = 0; it != end; ++it) {
			xmlpp::Element *option_node = dynamic_cast<xmlpp::Element *>(*it);

			if (option_node) {
				xmlpp::ustring tag = option_node->get_name();

				if ("option" == tag) {
					unsigned char *value = NULL;
					unsigned char *label = NULL;
					add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

					xmlpp::Attribute *disabled = option_node->get_attribute("disabled");

					if (disabled) {
						++i;
						continue;
					}

					xmlpp::Attribute *selected_attr = option_node->get_attribute("selected");
					bool selected = (selected_attr != nullptr);

					if (-1 == preselect && selected) {
						preselect = order;
					}
					xmlpp::ustring value_value = option_node->get_attribute_value("value");
					if (true) {
						value = memacpy(value_value.c_str(), value_value.size());

						if (!mem_align_alloc(&values, i, i + 1, 0xFF)) {
							if (lbl.source) done_string(&lbl);
							if (orig_lbl.source) done_string(&orig_lbl);
							if (values) {
								int j;

								for (j = 0; j < order; j++)
									mem_free_if(values[j]);
								mem_free(values);
							}
							destroy_menu(&lnk_menu);
							return;

						}
						values[order++] = value;
					}

					xmlpp::ustring label_value = option_node->get_attribute_value("label");
					if (label_value != "") {
						label = memacpy(label_value.c_str(), label_value.size());
					}
					if (label) {
						new_menu_item(&lnk_menu, label, order - 1, 0);
					}
					if (!value || !label) {
						init_string(&lbl);
						init_string(&orig_lbl);
						nnmi = !!label;
					}

					auto child_options = option_node->get_children();
					auto it2 = child_options.begin();
					auto end2 = child_options.end();
					xmlpp::ustring text_value;
					for (;it2 != end2; ++it2) {
						xmlpp::TextNode *text_node = dynamic_cast<xmlpp::TextNode *>(*it2);

						if (text_node) {
							text_value = text_node->get_content();
							break;
						}
					}

					if (text_value != "") {
						add_bytes_to_string(&lbl, text_value.c_str(), text_value.size());
						add_bytes_to_string(&orig_lbl, text_value.c_str(), text_value.size());
					}

//	if (!c_strlcasecmp(name, namelen, "OPTION", 6)) {
//		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);
//
//		if (!closing_tag) {
//			unsigned char *value, *label;

//			if (has_attr(t_attr, "disabled", html_context->doc_cp))
//				goto see;
//			if (preselect == -1
//			    && has_attr(t_attr, "selected", html_context->doc_cp))
//				preselect = order;
//			value = get_attr_val(t_attr, "value", html_context->doc_cp);

//			if (!mem_align_alloc(&values, order, order + 1, 0xFF))
//				goto abort;

//			values[order++] = value;
//			label = get_attr_val(t_attr, "label", html_context->doc_cp);
//			if (label) new_menu_item(&lnk_menu, label, order - 1, 0);
//			if (!value || !label) {
//				init_string(&lbl);
//				init_string(&orig_lbl);
//				nnmi = !!label;
//			}
//		}

//		goto see;
//	}
				++i;
				} else if ("optgroup" == tag) {
					unsigned char *label = NULL;

					add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);
					if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;

					xmlpp::ustring label_value = option_node->get_attribute_value("label");
					if (label_value != "") {
						label = memacpy(label_value.c_str(), label_value.size());
					}
					if (!label) {
						label = stracpy("");
						if (!label) {
							continue;
						}
					}
					new_menu_item(&lnk_menu, label, -1, 0);
					group = 1;
					++i;
				}
				if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;
			}
		}
	}
	add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

//	if (!c_strlcasecmp(name, namelen, "OPTGROUP", 8)) {
//		add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

//		if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;

//		if (!closing_tag) {
//			unsigned char *label;

//			label = get_attr_val(t_attr, "label", html_context->doc_cp);

//			if (!label) {
//				label = stracpy("");
//				if (!label) goto see;
//			}
//			new_menu_item(&lnk_menu, label, -1, 0);
//			group = 1;
//		}
//	}

//	goto see;


//end_parse:
//	*end = en;
	if (!order) {
//		*end = html;
		if (lbl.source) done_string(&lbl);
		if (orig_lbl.source) done_string(&orig_lbl);
		if (values) {
			int j;

			for (j = 0; j < order; j++)
				mem_free_if(values[j]);
			mem_free(values);
		}
		destroy_menu(&lnk_menu);
		return;
	}

	labels = (unsigned char **)mem_calloc(order, sizeof(unsigned char *));
	if (!labels) {
		if (lbl.source) done_string(&lbl);
		if (orig_lbl.source) done_string(&orig_lbl);
		if (values) {
			int j;

			for (j = 0; j < order; j++)
				mem_free_if(values[j]);
			mem_free(values);
		}
		destroy_menu(&lnk_menu);

		return;
	}

	fc = tags_init_form_control(FC_SELECT, node, html_context);
	if (!fc) {
		mem_free(labels);

		if (lbl.source) done_string(&lbl);
		if (orig_lbl.source) done_string(&orig_lbl);
		if (values) {
			int j;

			for (j = 0; j < order; j++)
				mem_free_if(values[j]);
			mem_free(values);
		}
		destroy_menu(&lnk_menu);

		return;
	}

	xmlpp::ustring id_value = select->get_attribute_value("id");
	if (id_value != "") {
		fc->id = memacpy(id_value.c_str(), id_value.size());
	}

	xmlpp::ustring name_value = select->get_attribute_value("name");
	if (name_value != "") {
		fc->name = memacpy(name_value.c_str(), name_value.size());
	}
	
//	fc->id = get_attr_val(attr, "id", html_context->doc_cp);
//	fc->name = get_attr_val(attr, "name", html_context->doc_cp);
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(values[fc->default_state]) : stracpy("");
	fc->nvalues = order;
	fc->values = values;
	fc->menu = detach_menu(&lnk_menu);
	fc->labels = labels;

	menu_labels(fc->menu, "", labels);
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	elformat.form = fc;
	elformat.style.attr |= AT_BOLD;
	put_chrs(html_context, "[&nbsp;", 7);

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

	put_chrs(html_context, "&nbsp;]", 7);
	pop_html_element(html_context);
	html_context->special_f(html_context, SP_CONTROL, fc);
}

void
tags_html_select(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	xmlpp::Element *select = node;
	xmlpp::ustring multiple = select->get_attribute_value("multiple");

	renderer->html_context->skip_select = 1;

	if (multiple != "") {
		do_tags_html_select_multiple(renderer, node, a, xxx3, xxx4, xxx5);
	} else {
		do_tags_html_select(renderer, node, a, xxx3, xxx4, xxx5);		
	}

//	if (has_attr(a, "multiple", html_context->doc_cp))
//		do_html_select_multiple(html_context, a, html, eof, end);
//	else
//		do_html_select(a, html, eof, end, html_context);
}

void
tags_html_select_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	renderer->html_context->skip_select = 0;
}

void
tags_html_small(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_small_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_source(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	char *url = NULL;
	char *prefix = "";
	void *parent;
	/* This just places a link where a video element would be. */

	xmlpp::Element *image = node;
	xmlpp::ustring src_value = image->get_attribute_value("src");
	if (src_value != "") {
		url = memacpy(src_value.c_str(), src_value.size());
	}
	
	//url = get_url_val(a, "src", html_context->doc_cp);
	if (!url) return;

	tags_html_focusable(renderer, node);

	xmlpp::Element *parent_node = image->get_parent();
	if (parent_node) {
		xmlpp::ustring tag_value = parent_node->get_name();
		if (tag_value == "audio") {
			prefix = "Audio: ";
		} else if (tag_value == "video") {
			prefix = "Video: ";
		}
	}
	
	put_link_line(prefix, basename(url), url,
              html_context->options->framename, html_context);

	//html_skip(html_context, a);

	mem_free(url);
}

void
tags_html_source_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_span(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_span_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_strong(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_strong_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_style(struct source_renderer *renderer, void *node, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	html_context->was_style = 1;
	html_context->skip_html = 1;

#ifdef CONFIG_CSS
	if (html_context->options->css_enable) {
		unsigned char *media = NULL;
		xmlpp::Element *element = node;
		xmlpp::ustring media_value = element->get_attribute_value("media");

		if (media_value != "") {
			media = memacpy(media_value.c_str(), media_value.size());
		}
//		unsigned char *media
//			= get_attr_val(attr, "media", html_context->doc_cp);
		html_context->support_css = supports_html_media_attr(media);
		mem_free_if(media);

//		if (support)
//			css_parse_stylesheet(&html_context->css_styles,
//					     html_context->base_href,
//					     html, eof);
	}
#endif




	//html_skip(html_context, a);	
}

void
tags_html_style_close(struct source_renderer *renderer, void *node, unsigned char *a,
                 unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	html_context->was_style = 0;
	html_context->skip_html = 0;
	html_context->support_css = 0;
}

void
tags_html_sub(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, "[", 1);	
}

void
tags_html_sub_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, "]", 1);
}

void
tags_html_summary(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_summary_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_sup(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, "^", 1);	
}

void
tags_html_sup_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_table(struct source_renderer *renderer, void *no, unsigned char *attr,
           unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	if (html_context->options->tables
	    && html_context->table_level < HTML_MAX_TABLE_LEVEL) {
		tags_format_table(renderer, no);
		ln_break(html_context, 2);

		return;
	}
	par_elformat.leftmargin = par_elformat.rightmargin = html_context->margin;
	par_elformat.align = ALIGN_LEFT;

	xmlpp::ustring align_value = node->get_attribute_value("align");
	if (align_value != "") {
		unsigned char *al = memacpy(align_value.c_str(), align_value.size());
		tags_html_linebrk(renderer, al);
	}
	elformat.style.attr = 0;
}

void
tags_html_table_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tbody(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tbody_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_td(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/*html_linebrk(html_context, a, html, eof, end);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	elformat.style.attr &= ~AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
tags_html_td_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_template(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_template_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_textarea(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	struct el_form_control *fc;
//	unsigned char *p, *t_name, *wrap_attr;
//	int t_namelen;
	int cols, rows;
	int i;
	xmlpp::Element *textarea = node;

	tags_html_focusable(renderer, node);

	fc = tags_init_form_control(FC_TEXTAREA, node, html_context);
	if (!fc) return;

	xmlpp::ustring id_value = textarea->get_attribute_value("id");
	if (id_value != "") {
		fc->id = memacpy(id_value.c_str(), id_value.size());
	}
//	fc->id = get_attr_val(attr, "id", html_context->doc_cp);

	xmlpp::ustring name_value = textarea->get_attribute_value("name");
	if (name_value != "") {
		fc->name = memacpy(name_value.c_str(), name_value.size());
	}
//	fc->name = get_attr_val(attr, "name", html_context->doc_cp);

	xmlpp::ustring default_value;
	xmlpp::TextNode *textNode = dynamic_cast<xmlpp::TextNode *>(textarea->get_first_child());
	if (textNode) {
		default_value = textNode->get_content();
	}
	if (true) {
		fc->default_value = memacpy(default_value.c_str(), default_value.size());
	}

	cols = 0;
	xmlpp::ustring cols_value = textarea->get_attribute_value("cols");
	cols = atoi(cols_value.c_str());
	if (cols <= 0)
		cols = html_context->options->default_form_input_size;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > html_context->options->document_width)
		cols = html_context->options->document_width;
	fc->cols = cols;

	rows = 0;
	xmlpp::ustring rows_value = textarea->get_attribute_value("rows");
	rows = atoi(rows_value.c_str());
//	rows = get_num(attr, "rows", html_context->doc_cp);
	if (rows <= 0) rows = 1;
	if (rows > html_context->options->box.height)
		rows = html_context->options->box.height;
	fc->rows = rows;
	html_context->options->needs_height = 1;

	fc->wrap = FORM_WRAP_SOFT;
	fc->maxlength = -1;
	xmlpp::ustring maxlength_value = textarea->get_attribute_value("maxlength");
	if (maxlength_value != "") {
		fc->maxlength = atoi(maxlength_value.c_str());
	}
//	fc->maxlength = get_num(attr, "maxlength", html_context->doc_cp);
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;

	if (rows > 1) ln_break(html_context, 1);
	else put_chrs(html_context, " ", 1);

	html_stack_dup(html_context, ELEMENT_KILLABLE);
	elformat.form = fc;
	elformat.style.attr |= AT_BOLD;

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
	html_context->skip_textarea = 1;
	html_context->special_f(html_context, SP_CONTROL, fc);
}

void
tags_html_textarea_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;

	html_context->skip_textarea = 0;
}

void
tags_html_tfoot(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tfoot_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_th(struct source_renderer *renderer, void *node, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/*html_linebrk(html_context, a, html, eof, end);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	elformat.style.attr |= AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
tags_html_th_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_thead(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_thead_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_time(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_time_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_title(struct source_renderer *renderer, void *node, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	html_top->invisible = 1;
	html_top->type = ELEMENT_WEAK;
}

void
tags_html_title_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tr(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	xmlpp::Element *node = no;
	xmlpp::ustring align_value = node->get_attribute_value("align");
	if (align_value != "") {
		unsigned char *al = memacpy(align_value.c_str(), align_value.size());
		tags_html_linebrk(renderer, al);
	}
}

void
tags_html_tr_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_track(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_track_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}
//
//void
//tags_html_tt(struct source_renderer *renderer, void *node, unsigned char *a,
//        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
//{
//}

void
tags_html_u(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	elformat.style.attr |= AT_UNDERLINE;
}

void
tags_html_u_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ul(struct source_renderer *renderer, void *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *node = no;
	unsigned char *al = NULL;

	/* dump_html_stack(html_context); */
	par_elformat.list_level++;
	par_elformat.list_number = 0;
	par_elformat.flags = P_DISC;

	xmlpp::ustring type_value = node->get_attribute_value("type");
	if (type_value != "") {
		al = memacpy(type_value.c_str(), type_value.size());
	}
	
	if (al) {
		if (!c_strcasecmp(al, "disc"))
			par_elformat.flags = P_DISC;
		else if (!c_strcasecmp(al, "circle"))
			par_elformat.flags = P_O;
		else if (!c_strcasecmp(al, "square"))
			par_elformat.flags = P_SQUARE;
		mem_free(al);
	}
	par_elformat.leftmargin += 2 + (par_elformat.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_elformat.leftmargin, par_elformat.width / 2);

	par_elformat.align = ALIGN_LEFT;
	html_top->type = ELEMENT_DONT_KILL;
}

void
tags_html_ul_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_var(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_var_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_video(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_video_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_wbr(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_wbr_close(struct source_renderer *renderer, void *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

//void
//tags_html_fixed(struct source_renderer *renderer, void *node, unsigned char *a,
//           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
//{
//	elformat.style.attr |= AT_FIXED;
//}




/* Handles meta tags in the HTML body. */
void
tags_html_handle_body_meta(struct source_renderer *renderer, unsigned char *meta,
		      unsigned char *eof)
{
	struct html_context *html_context = renderer->html_context;
	struct string head;

	if (!init_string(&head)) return;

	/* FIXME (bug 784): cp is the terminal charset;
	 * should use the document charset instead.  */
	scan_http_equiv(meta, eof, &head, NULL, html_context->options->cp);
	process_head(html_context, head.source);
	done_string(&head);
}

static void
tags_html_linebrk(struct source_renderer *renderer, unsigned char *al)
{
	struct html_context *html_context = renderer->html_context;

	if (al) {
		if (!c_strcasecmp(al, "left")) par_elformat.align = ALIGN_LEFT;
		else if (!c_strcasecmp(al, "right")) par_elformat.align = ALIGN_RIGHT;
		else if (!c_strcasecmp(al, "center")) {
			par_elformat.align = ALIGN_CENTER;
			if (!html_context->table_level)
				par_elformat.leftmargin = par_elformat.rightmargin = 0;
		} else if (!c_strcasecmp(al, "justify")) par_elformat.align = ALIGN_JUSTIFY;
		mem_free(al);
	}
}

static void
tags_html_h(int h, void *node, unsigned char *a,
       enum format_align default_align, struct source_renderer *renderer,
       unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	xmlpp::Element *element = node;

	if (!par_elformat.align) par_elformat.align = default_align;

	xmlpp::ustring align_value = element->get_attribute_value("align");

	if (align_value != "") {
		unsigned char *al = memacpy(align_value.c_str(), align_value.size());
		tags_html_linebrk(renderer, al);
	}

	h -= 2;
	if (h < 0) h = 0;

	switch (par_elformat.align) {
		case ALIGN_LEFT:
			par_elformat.leftmargin = h * 2;
			par_elformat.rightmargin = 0;
			break;
		case ALIGN_RIGHT:
			par_elformat.leftmargin = 0;
			par_elformat.rightmargin = h * 2;
			break;
		case ALIGN_CENTER:
			par_elformat.leftmargin = par_elformat.rightmargin = 0;
			break;
		case ALIGN_JUSTIFY:
			par_elformat.leftmargin = par_elformat.rightmargin = h * 2;
			break;
	}
}

void
tags_html_xmp(struct source_renderer *renderer, void *node, unsigned char *a,
         unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	html_context->was_xmp = 1;
	tags_html_pre(renderer, node, a, html, eof, end);
}

void
tags_html_xmp_close(struct source_renderer *renderer, void *node, unsigned char *a,
               unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	html_context->was_xmp = 0;
}

void
tags_html_nop(struct source_renderer *renderer, void *node, unsigned char *a,
               unsigned char *html, unsigned char *eof, unsigned char **end)
{
}

struct element_info2 tags_dom_html_array[] = {
	{"", 0, NULL, NULL, 0, ET_NON_NESTABLE}, /* _UNKNOWN */
	{"a", 1, tags_html_a, NULL, 0, ET_NON_NESTABLE}, /* A */
	{"abbr", 4, tags_html_abbr, NULL, 0, ET_NESTABLE}, /* ABBR */
	{"address",7, tags_html_address, NULL, 2, ET_NESTABLE}, /* ADDRESS */
	{"applet", 6, tags_html_applet, NULL, 1, ET_NON_PAIRABLE }, /* APPLET */
	{"area", 4, tags_html_area, tags_html_area_close, 0x0, 0x0}, /* AREA */
	{"article", 7, tags_html_article, tags_html_article_close, 0x0, 0x0}, /* ARTICLE */
	{"aside", 5, tags_html_aside, tags_html_aside_close, 0x0, 0x0}, /* ASIDE */
	{"audio", 5, tags_html_audio, NULL, 1, ET_NON_PAIRABLE}, /* AUDIO */
	{"b", 1, tags_html_b, NULL, 0, ET_NESTABLE}, /* B */
	{"base", 4, tags_html_base, NULL, 0, ET_NON_PAIRABLE}, /* BASE */
	{"basefont", 8, tags_html_basefont, NULL, 0, ET_NON_PAIRABLE}, /* BASEFONT */
	{"bdi", 3, tags_html_bdi, tags_html_bdi_close, 0x0, 0x0}, /* BDI */
	{"bdo", 3, tags_html_bdo, tags_html_bdo_close, 0x0, 0x0}, /* BDO */
	{"blockquote", 10, tags_html_blockquote, NULL, 2, ET_NESTABLE}, /* BLOCKQUOTE */
	{"body", 4, tags_html_body, NULL, 0, ET_NESTABLE}, /* BODY */
	{"br", 2, tags_html_br, NULL, 1, ET_NON_PAIRABLE}, /* BR */
	{"button", 6, tags_html_button, NULL, 0, ET_NESTABLE}, /* BUTTON */
	{"canvas", 6, tags_html_canvas, tags_html_canvas_close, 0x0, 0x0}, /* CANVAS */
	{"caption", 7, tags_html_caption, NULL, 1, ET_NESTABLE}, /* CAPTION */
	{"center", 6, tags_html_center, NULL, 1, ET_NESTABLE}, /* CENTER */
	{"cite", 4, tags_html_cite, tags_html_cite_close, 0x0, 0x0}, /* CITE */
	{"code", 4, tags_html_code, NULL, 0, ET_NESTABLE}, /* CODE */
	{"col", 3, tags_html_col, tags_html_col_close, 0x0, 0x0}, /* COL */
	{"colgroup", 8, tags_html_colgroup, tags_html_colgroup_close, 0x0, 0x0}, /* COLGROUP */
	{"data", 4, tags_html_data, tags_html_data_close, 0x0, 0x0}, /* DATA */
	{"datalist", 8, tags_html_datalist, tags_html_datalist_close, 0x0, 0x0}, /* DATALIST */
	{"dd", 2, tags_html_dd, NULL, 1, ET_NON_PAIRABLE}, /* DD */
	{"del", 3, tags_html_del, tags_html_del_close, 0x0, 0x0}, /* DEL */
	{"details", 7, tags_html_details, tags_html_details_close, 0x0, 0x0}, /* DETAILS */
	{"dfn", 3, tags_html_dfn, NULL, 0, ET_NESTABLE}, /* DFN */
	{"dialog", 6, tags_html_dialog, tags_html_dialog_close, 0x0, 0x0}, /* DIALOG */
	{"dir", 3, tags_html_dir, NULL, 2, ET_NESTABLE}, /* DIR */
	{"div", 3, tags_html_div, NULL, 1, ET_NESTABLE}, /* DIV */
	{"dl", 2, tags_html_dl, NULL, 2, ET_NESTABLE}, /* DL */
	{"dt", 2, tags_html_dt, NULL, 1, ET_NON_PAIRABLE}, /* DT */
	{"em", 2, tags_html_em, NULL, 0, ET_NESTABLE}, /* EM */
	{"embed", 5, tags_html_embed, NULL, 0, ET_NON_PAIRABLE}, /* EMBED */
	{"fieldset", 8, tags_html_fieldset, tags_html_fieldset_close, 0x0, 0x0}, /* FIELDSET */
	{"figcaption", 10, tags_html_figcaption, tags_html_figcaption_close, 0x0, 0x0}, /* FIGCAPTION */
	{"figure", 6, tags_html_figure, tags_html_figure_close, 0x0, 0x0}, /* FIGURE */
	{"font", 4, tags_html_font, NULL, 0, ET_NESTABLE}, /* FONT */
	{"footer", 6, tags_html_footer, tags_html_footer_close, 0x0, 0x0}, /* FOOTER */
	{"form", 4, tags_html_form, NULL, 1, ET_NESTABLE}, /* FORM */
	{"frame", 5, tags_html_frame, NULL, 1, ET_NON_PAIRABLE}, /* FRAME */
	{"frameset", 8, tags_html_frameset, NULL, 1, ET_NESTABLE}, /* FRAMESET */
	{"h1", 2, tags_html_h1, NULL, 2, ET_NON_NESTABLE}, /* H1 */
	{"h2", 2, tags_html_h2, NULL, 2, ET_NON_NESTABLE}, /* H2 */
	{"h3", 2, tags_html_h3, NULL, 2, ET_NON_NESTABLE}, /* H3 */
	{"h4", 2, tags_html_h4, NULL, 2, ET_NON_NESTABLE}, /* H4 */
	{"h5", 2, tags_html_h5, NULL, 2, ET_NON_NESTABLE}, /* H5 */
	{"h6", 2, tags_html_h6, NULL, 2, ET_NON_NESTABLE}, /* H6 */
	{"head", 4, tags_html_head, NULL, 0, ET_NESTABLE}, /* HEAD */
	{"header", 6, tags_html_header, tags_html_header_close, 0x0, 0x0}, /* HEADER */
	{"hgroup", 6, tags_html_hgroup, tags_html_hgroup_close, 0x0, 0x0}, /* HGROUP */
	{"hr", 2, tags_html_hr, NULL, 2, ET_NON_PAIRABLE}, /* HR */
	{"html", 4, tags_html_html, tags_html_html_close, 0, ET_NESTABLE}, /* HTML */
	{"i", 1, tags_html_i, NULL, 0, ET_NESTABLE}, /* I */
	{"iframe", 6, tags_html_iframe, NULL, 1, ET_NON_PAIRABLE}, /* IFRAME */
	{"img", 3, tags_html_img, NULL, 0, ET_NON_PAIRABLE}, /* IMG */
	{"input", 5, tags_html_input, NULL, 0, ET_NON_PAIRABLE}, /* INPUT */
	{"ins", 3, tags_html_ins, tags_html_ins_close, 0x0, 0x0}, /* INS */
	{"isindex", 7, tags_html_isindex, tags_html_isindex_close, 0x0, 0x0}, /* ISINDEX */
	{"kbd", 3, tags_html_kbd, tags_html_kbd_close, 0x0, 0x0}, /* KBD */
	{"keygen", 6, tags_html_keygen, tags_html_keygen_close, 0x0, 0x0}, /* KEYGEN */
	{"label", 5, tags_html_label, tags_html_label_close, 0x0, 0x0}, /* LABEL */
	{"legend", 6, tags_html_legend, tags_html_legend_close, 0x0, 0x0}, /* LEGEND */
	{"li", 2, tags_html_li, NULL, 1, ET_LI}, /* LI */
	{"link", 4, tags_html_link, NULL, 1, ET_NON_PAIRABLE}, /* LINK */
	{"main", 4, tags_html_main, tags_html_main_close, 0x0, 0x0}, /* MAIN */
	{"map", 3, tags_html_map, tags_html_map_close, 0x0, 0x0}, /* MAP */
	{"mark", 4, tags_html_mark, tags_html_mark_close, 0x0, 0x0}, /* MARK */
	{"menu", 4, tags_html_menu, NULL, 2, ET_NESTABLE}, /* MENU */
	{"menuitem", 8, tags_html_menuitem, tags_html_menuitem_close, 0x0, 0x0}, /* MENUITEM */
	{"meta", 4, tags_html_meta, NULL, 0, ET_NON_PAIRABLE}, /* META */
	{"meter", 5, tags_html_meter, tags_html_meter_close, 0x0, 0x0}, /* METER */
	{"nav", 3, tags_html_nav, tags_html_nav_close, 0x0, 0x0}, /* NAV */
	{"noscript", 8, tags_html_noscript, NULL, 0, ET_NESTABLE}, /* NOSCRIPT */
	{"object", 6, tags_html_object, NULL, 1, ET_NON_PAIRABLE}, /* OBJECT */
	{"ol", 2, tags_html_ol, NULL, 2, ET_NESTABLE}, /* OL */
	{"optgroup", 8, tags_html_optgroup, tags_html_optgroup_close, 0x0, 0x0}, /* OPTGROUP */
	{"option", 6, tags_html_option, NULL, 1, ET_NON_PAIRABLE}, /* OPTION */
	{"output", 6, tags_html_output, tags_html_output_close, 0x0, 0x0}, /* OUTPUT */
	{"p", 1, tags_html_p, NULL, 2, ET_NON_NESTABLE}, /* P */
	{"param", 5, tags_html_param, tags_html_param_close, 0x0, 0x0}, /* PARAM */
	{"picture", 7, tags_html_picture, tags_html_picture_close, 0x0, 0x0}, /* PICTURE */
	{"pre", 3, tags_html_pre, NULL, 2, ET_NESTABLE}, /* PRE */
	{"progress", 8, tags_html_progress, tags_html_progress_close, 0x0, 0x0}, /* PROGRESS */
	{"q", 1, tags_html_q, tags_html_q_close, 0, ET_NESTABLE}, /* Q */
	{"rp", 2, tags_html_rp, tags_html_rp_close, 0x0, 0x0}, /* RP */
	{"rt", 2, tags_html_rt, tags_html_rt_close, 0x0, 0x0}, /* RT */
	{"ruby", 4, tags_html_ruby, tags_html_ruby_close, 0x0, 0x0}, /* RUBY */
	{"s", 1, tags_html_s, NULL, 0, ET_NESTABLE}, /* S */
	{"samp", 4, tags_html_samp, tags_html_samp_close, 0x0, 0x0}, /* SAMP */
	{"script", 6, tags_html_script, NULL, 0, ET_NESTABLE}, /* SCRIPT */
	{"section", 7, tags_html_section, tags_html_section_close, 0x0, 0x0}, /* SECTION */
	{"select", 6, tags_html_select, tags_html_select_close, 0, ET_NESTABLE}, /* SELECT */
	{"small", 5, tags_html_small, tags_html_small_close, 0x0, 0x0}, /* SMALL */
	{"source", 6, tags_html_source, NULL, 1, ET_NON_PAIRABLE}, /* SOURCE */
	{"span", 4, tags_html_span, NULL, 0, ET_NESTABLE}, /* SPAN */
	{"strong", 6, tags_html_strong, NULL, 0, ET_NESTABLE}, /* STRONG */
	{"style", 5, tags_html_style, tags_html_style_close, 0, ET_NESTABLE}, /* STYLE */
	{"sub", 3, tags_html_sub, tags_html_sub_close, 0, ET_NESTABLE}, /* SUB */
	{"summary", 7, tags_html_summary, tags_html_summary_close, 0x0, 0x0}, /* SUMMARY */
	{"sup", 3, tags_html_sup, NULL, 0, ET_NESTABLE}, /* SUP */
	{"table", 5, tags_html_table, NULL, 2, ET_NESTABLE}, /* TABLE */
	{"tbody", 5, tags_html_tbody, tags_html_tbody_close, 0x0, 0x0}, /* TBODY */
	{"td", 2, tags_html_td, NULL, 0, ET_NESTABLE}, /* TD */
	{"template", 8, tags_html_template, tags_html_template_close, 0x0, 0x0}, /* TEMPLATE */
	{"textarea", 8, tags_html_textarea, tags_html_textarea_close, 0, 0x0}, /* TEXTAREA */
	{"tfoot", 5, tags_html_tfoot, tags_html_tfoot_close, 0x0, 0x0}, /* TFOOT */
	{"th", 2, tags_html_th, NULL, 0, ET_NESTABLE}, /* TH */
	{"thead", 5, tags_html_thead, tags_html_thead_close, 0x0, 0x0}, /* THEAD */
	{"time", 4, tags_html_time, tags_html_time_close, 0x0, 0x0}, /* TIME */
	{"title", 5, tags_html_title, NULL, 0, ET_NESTABLE}, /* TITLE */
	{"tr", 2, tags_html_tr, NULL, 1, ET_NESTABLE}, /* TR */
	{"track", 5, tags_html_track, tags_html_track_close, 0x0, 0x0}, /* TRACK */
	{"u", 1, tags_html_u, NULL, 0, ET_NESTABLE}, /* U */
	{"ul", 2, tags_html_ul, NULL, 2, ET_NESTABLE}, /* UL */
	{"var", 3, tags_html_var, tags_html_var_close, 0x0, 0x0}, /* VAR */
	{"video", 5, tags_html_video, NULL, 1, ET_NON_PAIRABLE}, /* VIDEO */
	{"wbr", 3, tags_html_wbr, tags_html_wbr_close, 0x0, 0x0}, /* WBR */
	{"xmp", 3, tags_html_xmp, tags_html_xmp_close, 0x0, 0x0} /* XMP */
};

#define NUMBER_OF_TAGS_2 (sizeof_array(tags_dom_html_array) - 1)

static int
compar(const void *a, const void *b)
{
	return c_strcasecmp(((struct element_info2 *) a)->name,
			    ((struct element_info2 *) b)->name);
}

struct element_info2 *
get_tag_value(unsigned char *name, int namelen)
{
	struct element_info2 *ei;

///#ifndef USE_FASTFIND
#if 1
	{
		struct element_info2 elem;
		unsigned char tmp;

		tmp = name[namelen];
		name[namelen] = '\0';

		elem.name = name;
		ei = bsearch(&elem, tags_dom_html_array, NUMBER_OF_TAGS_2, sizeof(elem), compar);
		name[namelen] = tmp;
	}
#else
	ei = (struct element_info2 *) fastfind_search(&ff_tags_index, name, namelen);
#endif

	return ei;
}

void
start_element_2(struct element_info2 *ei, struct source_renderer *renderer, void *nod)
{
	xmlpp::Element *node = nod;
	struct html_context *html_context = renderer->html_context;

#define ELEMENT_RENDER_PROLOGUE \
	ln_break(html_context, ei->linebreak); \
	xmlpp::ustring id_value = node->get_attribute_value("id"); \
	if (id_value != "") { \
		a = memacpy(id_value.c_str(), id_value.size()); \
		if (a) { \
			html_context->special_f(html_context, SP_TAG, a); \
			mem_free(a); \
		} \
	}

	char *a;
	struct par_attrib old_format;
	int restore_format;
#ifdef CONFIG_CSS
	struct css_selector *selector = NULL;
#endif

	/* If the currently top element on the stack cannot contain other
	 * elements, pop it. */
	if (html_top->type == ELEMENT_WEAK) {
		pop_html_element(html_context);
	}

	/* If an element handler has temporarily disabled rendering by setting
	 * the invisible flag, skip all handling for this element.
	 *
	 * As a special case, invisible can be set to 2 or greater to indicate
	 * that processing has been temporarily disabled except when a <script>
	 * tag is encountered. This special case is necessary for 2 situations:
	 * 1. A <script> tag is contained by a block with the CSS "display"
	 * property set to "none"; or 2. one <script> tag is nested inside
	 * another, but the outer script is not processed, in which case ELinks
	 * should still run any inner script blocks.  */
	if (html_top->invisible
	    && (ei->open != tags_html_script || html_top->invisible < 2)) {
		ELEMENT_RENDER_PROLOGUE
		return; // html;
	}

	/* Store formatting information for the currently top element on the
	 * stack before processing the new element. */
	restore_format = html_is_preformatted();
	old_format = par_elformat;

	/* Check for <meta refresh="..."> and <meta> cache-control directives
	 * inside <body> (see bug 700). */
	if (ei->open == tags_html_meta && html_context->was_body) {
///		tags_html_handle_body_meta(html_context, name - 1, eof);
		html_context->was_body = 0;
	}

	/* If this is a style tag, parse it. */
#ifdef CONFIG_CSS
	if (ei->open == tags_html_style && html_context->options->css_enable) {
		xmlpp::ustring media_value = node->get_attribute_value("media");
		char *media = NULL;

		if (media_value != "") {
			media = memacpy(media_value.c_str(), media_value.size());
		}

		int support = supports_html_media_attr(media);
		mem_free_if(media);

///		if (support)
///			css_parse_stylesheet(&html_context->css_styles,
///					     html_context->base_href,
///					     html, eof);
	}
#endif

	/* If this element is inline, non-nestable, and not <li>, and the next
	 * stack item down is the same element, try to pop both elements.
	 *
	 * The effect is to close automatically any <a> or <tt> tag that
	 * encloses the current tag if it is of the same element.
	 *
	 * Otherwise, if this element is non-inline or <li> and is
	 * non-nestable, search down through the stack until 1. we find a
	 * non-killable element; 2. the element being processed is not <li> and
	 * we find a block; or 3. the element being processed is <li> and we
	 * find another <li>.  Then if the element found is the same as the
	 * current element, try to pop all elements down to and including the
	 * found element.
	 *
	 * The effect is to close automatically any <hN>, <p>, or <li> tag that
	 * encloses the current tag if it is of the same element, ignoring any
	 * intervening inline elements.
	 */
	if (ei->type == ET_NON_NESTABLE || ei->type == ET_LI) {
		struct html_element *e;

		if (ei->type == ET_NON_NESTABLE) {
			foreach (e, html_context->stack) {
				if (e->type < ELEMENT_KILLABLE) break;
				if (is_block_element(e) || is_inline_element(ei)) break;
			}
		} else { /* This is an <li>. */
			foreach (e, html_context->stack) {
				if (is_block_element(e) && is_inline_element(ei)) break;
				if (e->type < ELEMENT_KILLABLE) break;
				if (!c_strlcasecmp(e->name, e->namelen, ei->name, ei->namelen)) break;
			}
		}

		if (!c_strlcasecmp(e->name, e->namelen, ei->name, ei->namelen)) {
			while (e->prev != (void *) &html_context->stack)
				kill_html_stack_item(html_context, e->prev);

			if (e->type > ELEMENT_IMMORTAL)
				kill_html_stack_item(html_context, e);
		}
	}

	/* Create an item on the stack for the element being processed. */
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	html_top->name = ei->name;
	html_top->namelen = ei->namelen;
	html_top->options = NULL;//attr;
	html_top->node = node;
	html_top->linebreak = ei->linebreak;

	/* If the element has an onClick handler for scripts, make it
	 * clickable. */
#ifdef CONFIG_ECMASCRIPT
	xmlpp::ustring onclick_value = node->get_attribute_value("onclick");

	if (onclick_value != "") {
		/* XXX: Put something better to elformat.link. --pasky */
		mem_free_set(&elformat.link, stracpy("javascript:void(0);"));
		mem_free_set(&elformat.target, stracpy(html_context->base_target));
		elformat.style.color.foreground = elformat.color.clink;
		html_top->pseudo_class = ELEMENT_LINK;
		mem_free_set(&elformat.title, stracpy("onClick placeholder"));
		/* Er. I know. Well, double html_focusable()s shouldn't
		 * really hurt. */
		tags_html_focusable(renderer, node);
	}
#endif

	/* Apply CSS styles. */
#ifdef CONFIG_CSS
	if (html_top->options && html_context->options->css_enable) {
		/* XXX: We should apply CSS otherwise as well, but that'll need
		 * some deeper changes in order to have options filled etc.
		 * Probably just applying CSS from more places, since we
		 * usually have type != ET_NESTABLE when we either (1)
		 * rescan on your own from somewhere else (2) html_stack_dup()
		 * in our own way.  --pasky */
		xmlpp::ustring id_value = node->get_attribute_value("id");
		char *id = NULL;
		if (id_value != "") {
			id = memacpy(id_value.c_str(), id_value.size());
		}
		mem_free_set(&html_top->attr.id, id);

		xmlpp::ustring class_value = node->get_attribute_value("class");
		char *class_ = NULL;
		if (class_value != "") {
			class_ = memacpy(class_value.c_str(), class_value.size());
		}
		mem_free_set(&html_top->attr.class_, class_);
		/* Call it now to gain some of the stuff which might affect
		 * formatting of some elements. */
		/* FIXME: The caching of the CSS selector is broken, since t can
		 * lead to wrong styles being applied to following elements, so
		 * disabled for now. */
		selector = get_css_selector_for_element(html_context, html_top,
							&html_context->css_styles,
							&html_context->stack);

		if (selector) {
			apply_css_selector_style(html_context, html_top, selector);
			done_css_selector(selector);
		}
	}
#endif

	/* 1. Put any linebreaks that the element calls for, and 2. register
	 * any fragment identifier.  Step 1 cannot be done before applying CSS
	 * styles because the CSS "display" property may change the element's
	 * linebreak value.
	 *
	 * XXX: The above is wrong: ELEMENT_RENDER_PROLOGUE only looks at the
	 * linebreak value for the element_info structure, which CSS cannot
	 * modify.  -- Miciah */
	ELEMENT_RENDER_PROLOGUE

	/* Call the element's open handler. */
	if (ei->open) ei->open(renderer, node, NULL, NULL, NULL, NULL);

	/* Apply CSS styles again. */
#ifdef CONFIG_CSS
	if (selector && html_top->options) {
		/* Call it now to override default colors of the elements. */
		selector = get_css_selector_for_element(html_context, html_top,
							&html_context->css_styles,
							&html_context->stack);

		if (selector) {
			apply_css_selector_style(html_context, html_top, selector);
			done_css_selector(selector);
		}
	}
#endif

	/* If this element was not <br>, clear the was_br flag. */
	if (ei->open != tags_html_br) html_context->was_br = 0;

	/* If this element is not pairable, pop its stack item now. */
	if (ei->type == ET_NON_PAIRABLE)
		kill_html_stack_item(html_context, html_top);

	/* If we are rendering preformatted text (see above), restore the
	 * formatting attributes that were in effect before processing this
	 * element. */
	if (restore_format) par_elformat = old_format;

	return;
#undef ELEMENT_RENDER_PROLOGUE
}

void
end_element_2(struct element_info2 *ei, struct source_renderer *renderer, void *nod)
{
	xmlpp::Element *node = nod;
	struct html_context *html_context = renderer->html_context;
	struct html_element *e, *elt;
	int lnb = 0;
	int kill = 0;

	html_context->putsp = HTML_SPACE_ADD;
	html_context->was_br = 0;

	/* If this was a non-pairable tag or an <li>; perform no further
	 * processing. */
	if (ei->type == ET_NON_PAIRABLE /* || ei->type == ET_LI */)
		return;

	/* Call the element's close handler. */
	if (ei->close) ei->close(renderer, node, NULL, NULL, NULL, NULL);

	/* dump_html_stack(html_context); */

	/* Search down through the stack until we find 1. a different element
	 * that cannot be killed or 2. the element that is currently being
	 * processed (NOT necessarily the same instance of that element).
	 *
	 * In the first case, we are done.  In the second, if this is an inline
	 * element and we found a block element while searching, we kill the
	 * found element; else (either this is inline but no block was found or
	 * this is a block), output linebreaks for all of the elements down to
	 * and including the found element and then pop all of these elements.
	 *
	 * The effects of the procedure outlined above and implemented below
	 * are 1. to allow an inline element to close any elements that it
	 * contains iff the inline element does not contain any blocks and 2.
	 * to allow blocks to close any elements that they contain.
	 *
	 * Two situations in which this behaviour may not match expectations
	 * are demonstrated by the following HTML:
	 *
	 *    <b>a<a href="file:///">b</b>c</a>d
	 *    <a href="file:///">e<b>f<div>g</a>h</div></b>
	 *
	 * ELinks will render "a" as bold text; "b" as bold, hyperlinked text;
	 * both "c" and "d" as normal (non-bold, non-hyperlinked) text (note
	 * that "</b>" closed the link because "<b>" enclosed the "<a>" open
	 * tag); "e" as hyperlinked text; and "f", "g", and "h" all as bold,
	 * hyperlinked text (note that "</a>" did not close the link because
	 * "</a>" was enclosed by "<div>", a block tag, while the "<a>" open
	 * tag was outside of the "<div>" block).
	 *
	 * Note also that close handlers are not called, which might also lead
	 * to unexpected behaviour as illustrated by the following example:
	 *
	 *    <b><q>I like fudge,</b> he said. <q>So do I,</q> she said.
	 *
	 * ELinks will render "I like fudge," with bold typeface but will only
	 * place an opening double-quotation mark before the text and no closing
	 * mark.  "he said." will be rendered normally.  "So do I," will be
	 * rendered using single-quotation marks (as for a quotation within a
	 * quotation).  "she said." will be rendered normally.  */
	foreach (e, html_context->stack) {
		if (is_block_element(e) && is_inline_element(ei)) kill = 1;
		if (c_strlcasecmp(e->name, e->namelen, ei->name, ei->namelen)) {
			if (e->type < ELEMENT_KILLABLE)
				break;
			else
				continue;
		}
		if (kill) {
			kill_html_stack_item(html_context, e);
			break;
		}
		for (elt = e;
		     elt != (void *) &html_context->stack;
		     elt = elt->prev)
			if (elt->linebreak > lnb)
				lnb = elt->linebreak;

		/* This hack forces a line break after a list end. It is needed
		 * when ending a list with the last <li> having no text the
		 * line_breax is 2 so the ending list's linebreak will be
		 * ignored when calling ln_break(). */
		if (html_context->was_li)
			html_context->line_breax = 0;

		ln_break(html_context, lnb);
		while (e->prev != (void *) &html_context->stack)
			kill_html_stack_item(html_context, e->prev);
		kill_html_stack_item(html_context, e);
		break;
	}

	/* dump_html_stack(html_context); */

	return;
}