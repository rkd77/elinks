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
#include <dom/dom.h>
#if 0
#include <dom/html/html_base_element.h>
#include <dom/html/html_body_element.h>
#include <dom/html/html_dlist_element.h>
#include <dom/html/html_element.h>
#include <dom/html/html_font_element.h>
#include <dom/html/html_frame_element.h>
#include <dom/html/html_frameset_element.h>
#include <dom/html/html_hr_element.h>
#include <dom/html/html_li_element.h>
#include <dom/html/html_olist_element.h>
#include <dom/html/html_script_element.h>
#include <dom/html/html_ulist_element.h>
#endif

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
#include "document/libdom/tags.h"
#include "document/options.h"
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

static struct el_form_control *
tags_init_form_control(enum form_type type, unsigned char *attr,
                  struct html_context *html_context)
{
	struct el_form_control *fc;

	fc = mem_calloc(1, sizeof(*fc));
	if (!fc) return NULL;

	fc->type = type;
	fc->position = ++html_context->ff;
//	fc->position = attr - html_context->startf;
//	fc->mode = get_form_mode(html_context, attr);

	return fc;
}

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

	if (par_format.color.background != format.style.color.background) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_bottom;

		html_context->was_body_background = 1;
		e->parattr.color.background = e->attr.style.color.background = par_format.color.background = format.style.color.background;
	}

	if (html_context->has_link_lines
	    && par_format.color.background != html_context->options->default_style.color.background
	    && !search_html_stack(html_context, DOM_HTML_ELEMENT_TYPE_BODY)) {
		html_context->special_f(html_context, SP_COLOR_LINK_LINES);
	}
}

static void tags_html_linebrk(struct source_renderer *renderer, unsigned char *al);
static void tags_html_h(int h, dom_node *node, unsigned char *a,
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
tags_html_focusable(struct source_renderer *renderer, dom_node *node)
{
	struct html_context *html_context = renderer->html_context;
	dom_string *accesskey_value = NULL;
	int32_t tabindex;
	dom_exception exc;

	format.accesskey = 0;
	format.tabindex = 0x80000000;

	exc = dom_html_anchor_element_get_access_key((dom_html_anchor_element *)node, &accesskey_value);
	if (DOM_NO_ERR == exc && accesskey_value) {
		unsigned char *accesskey = memacpy(dom_string_data(accesskey_value), dom_string_byte_length(accesskey_value));
		dom_string_unref(accesskey_value);

		if (accesskey) {
			format.accesskey = accesskey_string_to_unicode(accesskey);
			mem_free(accesskey);
		}
	}

	exc = dom_html_anchor_element_get_tab_index((dom_html_anchor_element
	*)node, &tabindex);
	if (DOM_NO_ERR == exc) {
		if (0 < tabindex && tabindex < 32767) {
			format.tabindex = (tabindex & 0x7fff) << 16;
		}
	}
}

void
tags_html_a(struct source_renderer *renderer, dom_node *node, unsigned char *a,
       unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_string *href_value = NULL;
	dom_string *name_value = NULL;
	unsigned char *href;
	dom_html_anchor_element *anchor = (dom_html_anchor_element *)node;
	
	dom_exception exc = dom_html_anchor_element_get_href(anchor, &href_value);

	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for dom_html_anchor_element_get_href\n");
		return;
	}

	if (href_value == NULL) {
		/* Element lacks required attribute */
		return;
	}

	href = memacpy(dom_string_data(href_value), dom_string_byte_length(href_value));
	dom_string_unref(href_value);

	if (href) {
		dom_string *target_value = NULL;
		dom_string *title_value = NULL;
		unsigned char *target = NULL;
		unsigned char *title = NULL;

		mem_free_set(&format.link,
			     join_urls(html_context->base_href,
				       trim_chars(href, ' ', 0)));

		//fprintf(stderr, "tags_html_a: format.link=%s\n", format.link);


		mem_free(href);

		exc = dom_html_anchor_element_get_target(anchor, &target_value);

		if (exc != DOM_NO_ERR) {
			DBG("get_target failed");
		}

		if (target_value) {

			target = memacpy(dom_string_data(target_value), dom_string_byte_length(target_value));
			dom_string_unref(target_value);
		}

		if (target) {
			mem_free_set(&format.target, target);
		} else {
			mem_free_set(&format.target, stracpy(html_context->base_target));
		}

		if (0) {
			; /* Shut up compiler */
#ifdef CONFIG_GLOBHIST
		} else if (get_global_history_item(format.link)) {
			format.style.color.foreground = format.color.vlink;
			html_top->pseudo_class &= ~ELEMENT_LINK;
			html_top->pseudo_class |= ELEMENT_VISITED;
#endif
#ifdef CONFIG_BOOKMARKS
		} else if (get_bookmark(format.link)) {
			format.style.color.foreground = format.color.bookmark_link;
			html_top->pseudo_class &= ~ELEMENT_VISITED;
			/* XXX: Really set ELEMENT_LINK? --pasky */
			html_top->pseudo_class |= ELEMENT_LINK;
#endif
		} else {
			format.style.color.foreground = format.color.clink;
			html_top->pseudo_class &= ~ELEMENT_VISITED;
			html_top->pseudo_class |= ELEMENT_LINK;
		}

		exc = dom_html_element_get_title(anchor, &title_value);

		if (exc != DOM_NO_ERR) {
			DBG("Failed get_title");
			return;
		}

		if (title_value) {
			title = memacpy(dom_string_data(title_value), dom_string_byte_length(title_value));
			dom_string_unref(title_value);
		}

		mem_free_set(&format.title, title);

		tags_html_focusable(renderer, (dom_node *)anchor);

	} else {
		pop_html_element(html_context);
	}
	
	exc = dom_html_anchor_element_get_name(anchor, &name_value);
	if (DOM_NO_ERR == exc) {
		if (name_value) {
			unsigned char *name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
			dom_string_unref(name_value);

			tags_set_fragment_identifier(html_context, name);
		}
	}
	//set_fragment_identifier(html_context, a, "name");
}

void
tags_html_a_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_abbr(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_abbr_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_address(struct source_renderer *renderer, dom_node *node, unsigned char *a,
             unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	par_format.leftmargin++;
	par_format.align = ALIGN_LEFT;
}

void
tags_html_address_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_applet(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_applet_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_area(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_area_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_article(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_article_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_aside(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_aside_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_audio(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_audio_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_b(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	format.style.attr |= AT_BOLD;
}

void
tags_html_b_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_base(struct source_renderer *renderer, dom_node *no, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_html_base_element *element = (dom_html_base_element *)no;
	unsigned char *al;
	dom_string *href_value = NULL;
	dom_string *target_value = NULL;
	dom_exception exc = dom_html_base_element_get_href(element, &href_value);

	if (DOM_NO_ERR != exc) {
		DBG("get_href_failed");
		return;
	}

	if (href_value) {
		al = memacpy(dom_string_data(href_value), dom_string_byte_length(href_value));
		dom_string_unref(href_value);

		if (al) {
			unsigned char *base = join_urls(html_context->base_href, al);
			struct uri *uri = base ? get_uri(base, 0) : NULL;

			mem_free(al);
			mem_free_if(base);

			if (uri) {
				done_uri(html_context->base_href);
				html_context->base_href = uri;
			}
		}
	}
	exc = dom_html_base_element_get_target(element, &target_value);

	if (DOM_NO_ERR == exc) {
		if (target_value) {
			al = memacpy(dom_string_data(target_value), dom_string_byte_length(target_value));
			dom_string_unref(target_value);

			if (al) mem_free_set(&html_context->base_target, al);
		}
	}
}

void
tags_html_base_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_basefont(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_basefont_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdi(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdi_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdo(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_bdo_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_blockquote(struct source_renderer *renderer, dom_node *node, unsigned char *a,
                unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	par_format.leftmargin += 2;
	par_format.align = ALIGN_LEFT;
}

void
tags_html_blockquote_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
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

	if (par_format.color.background != format.style.color.background) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_bottom;

		html_context->was_body_background = 1;
		e->parattr.color.background = e->attr.style.color.background = par_format.color.background = format.style.color.background;
	}

	if (html_context->has_link_lines
	    && par_format.color.background != html_context->options->default_style.color.background
	    && !search_html_stack(html_context, DOM_HTML_ELEMENT_TYPE_BODY)) {
		html_context->special_f(html_context, SP_COLOR_LINK_LINES);
	}
}

void
tags_html_body(struct source_renderer *renderer, dom_node *no, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_string *text_value = NULL;
	dom_string *link_value = NULL;
	dom_string *vlink_value = NULL;
	dom_string *bgcolor_value = NULL;
	dom_html_body_element *node = (dom_html_body_element *)no;
	
	exc = dom_html_body_element_get_text(node, &text_value);
	if (DOM_NO_ERR == exc) {
		get_color2(html_context, text_value, &format.style.color.foreground);
		if (text_value) {
			dom_string_unref(text_value);
		}
	}

	exc = dom_html_body_element_get_a_link(node, &link_value);
	if (DOM_NO_ERR == exc) {
		get_color2(html_context, link_value, &format.color.clink);
		if (link_value) {
			dom_string_unref(link_value);
		}
	}

	exc = dom_html_body_element_get_v_link(node, &vlink_value);
	if (DOM_NO_ERR == exc) {
		get_color2(html_context, vlink_value, &format.color.vlink);
		if (vlink_value) {
			dom_string_unref(vlink_value);
		}
	}

	exc = dom_html_body_element_get_bg_color(node, &bgcolor_value);
	if (DOM_NO_ERR == exc) {
		int v = get_color2(html_context, bgcolor_value, &format.style.color.background);
		if (bgcolor_value) {
			dom_string_unref(bgcolor_value);
		}
		if (-1 != v) {
			html_context->was_body_background = 1;
		}
	}

	html_context->was_body = 1; /* this will be used by "meta inside body" */
	tags_html_apply_canvas_bgcolor(renderer);
}

void
tags_html_body_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_br(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_br_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_button(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *al = NULL;
	struct el_form_control *fc;
	enum form_type type = FC_SUBMIT;
	dom_string *type_value = NULL;
	dom_string *id_value = NULL;
	dom_string *name_value = NULL;
	dom_string *value_value = NULL;
	dom_exception exc;
	dom_html_button_element *button = (dom_html_button_element *)node;
	bool disabled = false;

	html_focusable(html_context, a);

	exc = dom_html_button_element_get_type(button, &type_value);
	if (DOM_NO_ERR == exc && type_value) {
		al = memacpy(dom_string_data(type_value), dom_string_byte_length(type_value));
		dom_string_unref(type_value);
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
	fc = tags_init_form_control(type, a, html_context);
	if (!fc) return;

	
	exc = dom_html_button_element_get_disabled(button, &disabled);
	if (DOM_NO_ERR == exc) {
		if (disabled) {
			fc->mode = FORM_MODE_DISABLED;
		}
	}
	
	exc = dom_html_element_get_id((dom_html_element *)node, &id_value);
	if (DOM_NO_ERR == exc && id_value) {
		fc->id = memacpy(dom_string_data(id_value), dom_string_byte_length(id_value));
		dom_string_unref(id_value);
	}
	
	//fc->id = get_attr_val(a, "id", cp);
	exc = dom_html_button_element_get_name(button, &name_value);
	if (DOM_NO_ERR == exc && name_value) {
		fc->name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
		dom_string_unref(name_value);
	}
	//fc->name = get_attr_val(a, "name", cp);
	
	exc = dom_html_button_element_get_value(button, &value_value);
	if (DOM_NO_ERR == exc && value_value) {
		fc->default_value = memacpy(dom_string_data(value_value), dom_string_byte_length(value_value));
		dom_string_unref(value_value);
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
	format.form = fc;
	format.style.attr |= AT_BOLD;
}

void
tags_html_button_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_canvas(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_canvas_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_caption(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_caption_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_center(struct source_renderer *renderer, dom_node *node, unsigned char *a,
            unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	par_format.align = ALIGN_CENTER;
	if (!html_context->table_level)
		par_format.leftmargin = par_format.rightmargin = 0;
}

void
tags_html_center_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_cite(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_cite_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_code(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_code_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_col(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_col_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_colgroup(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_colgroup_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_data(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_data_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_datalist(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_datalist_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dd(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	kill_html_stack_until(html_context, 0, "", "DL", NULL);

	par_format.leftmargin = par_format.dd_margin + 3;

	if (!html_context->table_level) {
		par_format.leftmargin += 5;
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);
	}
	par_format.align = ALIGN_LEFT;
}

void
tags_html_dd_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_del(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_del_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_details(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_details_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dfn(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dfn_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dialog(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dialog_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dir(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dir_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_div(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_div_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dl(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	bool compact = false;
	dom_html_dlist_element *node = (dom_html_dlist_element *)no;
	dom_exception exc = dom_html_dlist_element_get_compact(node, &compact);

	par_format.flags &= ~P_COMPACT;

	if (DOM_NO_ERR == exc) {
		if (compact) {
			par_format.flags |= P_COMPACT;
		}
	}

	if (par_format.list_level) par_format.leftmargin += 5;
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.align = ALIGN_LEFT;
	par_format.dd_margin = par_format.leftmargin;
	html_top->type = ELEMENT_DONT_KILL;

	if (!(par_format.flags & P_COMPACT)) {
		ln_break(html_context, 2);
		html_top->linebreak = 2;
	}
}

void
tags_html_dl_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_dt(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	bool compact = false;
	dom_html_dlist_element *node = (dom_html_dlist_element *)no;
	dom_exception exc = dom_html_dlist_element_get_compact(node, &compact);

	if (DOM_NO_ERR == exc) {
		kill_html_stack_until(html_context, 0, "", "DL", NULL);
		par_format.align = ALIGN_LEFT;
		par_format.leftmargin = par_format.dd_margin;

		if (!(par_format.flags & P_COMPACT) && !compact)
			ln_break(html_context, 2);
	}
}

void
tags_html_dt_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_em(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_em_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_embed(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_embed_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_fieldset(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_fieldset_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figcaption(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figcaption_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figure(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_figure_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_font(struct source_renderer *renderer, dom_node *no, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_string *size_value = NULL;
	dom_string *color_value = NULL;
	dom_html_font_element *node = (dom_html_font_element *)no;

	dom_exception exc = dom_html_font_element_get_size(node, &size_value);
    
	if (DOM_NO_ERR == exc) {
		if (size_value) {
			unsigned char *al = memacpy(dom_string_data(size_value), dom_string_byte_length(size_value));

			dom_string_unref(size_value);

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
					if (!p) format.fontsize = s;
					else format.fontsize += p * s;
					if (format.fontsize < 1) format.fontsize = 1;
					else if (format.fontsize > 7) format.fontsize = 7;
				}
				mem_free(al);
			}
		}
	}
	exc = dom_html_font_element_get_color(node, &color_value);
	if (DOM_NO_ERR == exc) {
		get_color2(html_context, color_value, &format.style.color.foreground);

		if (color_value) {
			dom_string_unref(color_value);
		}
	}
}

void
tags_html_font_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_footer(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_footer_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_form(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *al=NULL;
	struct form *form;
	dom_exception exc;
	dom_string *method_value = NULL;
	dom_string *enctype_value = NULL;
	dom_string *onsubmit;
	dom_string *onsubmit_value = NULL;
	dom_string *action_value = NULL;
	dom_string *name;
	dom_string *name_value = NULL;
	dom_string *target_value = NULL;
	dom_html_form_element *form_node = (dom_html_form_element *)node;
	
	html_context->was_br = 1;

	form = init_form();
	if (!form) return;

	form->method = FORM_METHOD_GET;
	form->form_num = ++html_context->ff;
//	form->form_num = a - html_context->startf;

	exc = dom_html_form_element_get_method(form_node, &method_value);
	if (DOM_NO_ERR == exc && method_value) {
		al = memacpy(dom_string_data(method_value), dom_string_byte_length(method_value));
		dom_string_unref(method_value);
	}
	
	//al = get_attr_val(a, "method", html_context->doc_cp);
	if (al) {
		if (!c_strcasecmp(al, "post")) {
			exc = dom_html_form_element_get_enctype(form_node, &enctype_value);
			if (DOM_NO_ERR == exc && enctype_value) {
				unsigned char *enctype = memacpy(dom_string_data(enctype_value), dom_string_byte_length(enctype_value));
				dom_string_unref(enctype_value);
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
	exc = dom_string_create("onsubmit", sizeof("onsubmit") - 1, &onsubmit);
	if (DOM_NO_ERR == exc) {
		exc = dom_element_get_attribute(node, onsubmit, &onsubmit_value);
		if (DOM_NO_ERR == exc && onsubmit_value) {
			form->onsubmit = memacpy(dom_string_data(onsubmit_value), dom_string_byte_length(onsubmit_value));
			dom_string_unref(onsubmit_value);
		}
		dom_string_unref(onsubmit);
	}
	
	//form->onsubmit = get_attr_val(a, "onsubmit", html_context->doc_cp);
	exc = dom_string_create("name", sizeof("name") - 1, &name);
	if (DOM_NO_ERR == exc) {
		exc = dom_element_get_attribute(node, name, &name_value);
		if (DOM_NO_ERR == exc && name_value) {
			form->name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
			dom_string_unref(name_value);
		}
		dom_string_unref(name);
	}
	//al = get_attr_val(a, "name", html_context->doc_cp);
	//if (al) form->name = al;

	exc = dom_html_form_element_get_action(form_node, &action_value);
	if (DOM_NO_ERR == exc && action_value) {
		al = memacpy(dom_string_data(action_value), dom_string_byte_length(action_value));
		dom_string_unref(action_value);
	
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
	exc = dom_html_form_element_get_target(form_node, &target_value);
	if (DOM_NO_ERR == exc && target_value) {
		al = memacpy(dom_string_data(target_value), dom_string_byte_length(target_value));
		dom_string_unref(target_value);
	}
	//al = get_target(html_context->options, a);
	form->target = al ? al : stracpy(html_context->base_target);

	html_context->special_f(html_context, SP_FORM, form);
}

void
tags_html_form_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_frame(struct source_renderer *renderer, dom_node *no, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_string *src_value = NULL;
	dom_string *name_value = NULL;
	dom_html_frame_element *node = (dom_html_frame_element *)no;
	dom_exception exc = dom_html_frame_element_get_src(node, &src_value);
	unsigned char *src = NULL, *name = NULL, *url;

	if (DOM_NO_ERR == exc) {
		if (src_value) {
			src = memacpy(dom_string_data(src_value), dom_string_byte_length(src_value));
			dom_string_unref(src_value);
		}
	}

	if (!src) {
		url = stracpy("about:blank");
	} else {
		url = join_urls(html_context->base_href, src);
		mem_free(src);
	}
	if (!url) return;

	exc = dom_html_frame_element_get_name(node, &name_value);

	if (DOM_NO_ERR == exc) {
		if (name_value) {
			name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
			dom_string_unref(name_value);
		}
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
		html_focusable(html_context, a);
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
tags_html_frame_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_frameset(struct source_renderer *renderer, dom_node *no, unsigned char *a,
              unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_string *cols_value = NULL;
	dom_string *rows_value = NULL;
	dom_html_frame_set_element *node = (dom_html_frame_set_element *)no;
	dom_exception exc;
	struct frameset_param fp;
	unsigned char *cols = NULL, *rows = NULL;
	int width, height;

	/* XXX: This is still not 100% correct. We should also ignore the
	 * frameset when we encountered anything 3v1l (read as: non-whitespace
	 * text/element/anything) in the document outside of <head>. Well, this
	 * is still better than nothing and it should heal up the security
	 * concerns at least because sane sites should enclose the documents in
	 * <body> elements ;-). See also bug 171. --pasky */
	if (search_html_stack(html_context, DOM_HTML_ELEMENT_TYPE_BODY)
	    || !html_context->options->frames
	    || !html_context->special_f(html_context, SP_USED, NULL))
		return;

	exc = dom_html_frame_set_element_get_cols(node, &cols_value);

	if (DOM_NO_ERR == exc) {
		if (cols_value) {
			cols = memacpy(dom_string_data(cols_value), dom_string_byte_length(cols_value));
			dom_string_unref(cols_value);
		}
	}
	
	if (!cols) {
		cols = stracpy("100%");
		if (!cols) return;
	}

	exc = dom_html_frame_set_element_get_rows(node, &rows_value);

	if (DOM_NO_ERR == exc) {
		if (rows_value) {
			rows = memacpy(dom_string_data(rows_value), dom_string_byte_length(rows_value));
			dom_string_unref(rows_value);
		}
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
tags_html_frameset_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h1(struct source_renderer *renderer, dom_node *node, unsigned char *a,
	unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	format.style.attr |= AT_BOLD;
	tags_html_h(1, node, a, ALIGN_CENTER, renderer, html, eof, end);
}

void
tags_html_h1_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h2(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(2, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h2_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h3(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(3, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h3_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h4(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(4, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h4_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h5(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(5, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h5_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_h6(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	tags_html_h(6, node, a, ALIGN_LEFT, renderer, html, eof, end);
}

void
tags_html_h6_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_head(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	/* This makes sure it gets to the stack and helps tame down unclosed
	 * <title>. */
	renderer->html_context->skip_html = 1;
}

void
tags_html_head_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	renderer->html_context->skip_html = 0;
}

void
tags_html_header(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_header_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_hgroup(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_hgroup_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_hr(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	int i = -1/* = par_format.width - 10*/;
	unsigned char r = (unsigned char) BORDER_DHLINE;
	dom_exception exc;
	dom_string *size_value;
	dom_string *width_value = NULL;
	dom_string *align_value = NULL;
	dom_html_hr_element *node = (dom_html_hr_element *)no;
	unsigned char *al = NULL;
	int q = -1;
	//dom_long q = 0;

	exc = dom_html_hr_element_get_size(node, &size_value);
	if (DOM_NO_ERR == exc) {
		if (size_value) {
			al = memacpy(dom_string_data(size_value), dom_string_byte_length(size_value));
			dom_string_unref(size_value);
		}
		q = get_num2(al);
	}
	
	if (q >= 0 && q < 2) r = (unsigned char) BORDER_SHLINE;
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	par_format.align = ALIGN_CENTER;
	mem_free_set(&format.link, NULL);
	format.form = NULL;

	exc = dom_html_hr_element_get_align(node, &align_value);
	if (DOM_NO_ERR == exc) {
		if (align_value) {
			al = memacpy(dom_string_data(align_value), dom_string_byte_length(align_value));
			dom_string_unref(align_value);
			tags_html_linebrk(renderer, al);
		}
	}
	if (par_format.align == ALIGN_JUSTIFY) par_format.align = ALIGN_CENTER;
	par_format.leftmargin = par_format.rightmargin = html_context->margin;

	exc = dom_html_hr_element_get_width(node, &width_value);
	if (DOM_NO_ERR == exc) {
		if (width_value) {
			al = memacpy(dom_string_data(width_value), dom_string_byte_length(width_value));
			dom_string_unref(width_value);
			i = get_width2(html_context, al, 1);
		}
	}

	if (i == -1) i = get_html_max_width();
	format.style.attr = AT_GRAPHICS;
	html_context->special_f(html_context, SP_NOWRAP, 1);
	while (i-- > 0) {
		put_chrs(html_context, &r, 1);
	}
	html_context->special_f(html_context, SP_NOWRAP, 0);
	ln_break(html_context, 2);
	pop_html_element(html_context);
}

void
tags_html_hr_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_html(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/* This is here just to get CSS stuff applied. */

	/* Modify the root HTML element - format_html_part() will take
	 * this from there. */
	struct html_element *e = html_bottom;

	if (par_format.color.background != format.style.color.background)
		e->parattr.color.background = e->attr.style.color.background = par_format.color.background = format.style.color.background;
}

void
tags_html_html_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
                unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	if (html_top->type >= ELEMENT_KILLABLE
	    && !html_context->was_body_background)
		html_apply_canvas_bgcolor(renderer);
}

void
tags_html_i(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	format.style.attr |= AT_ITALIC;
}

void
tags_html_i_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_iframe(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_iframe_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_img(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_img_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}


static void
tags_html_input_format(struct html_context *html_context, dom_node *node, unsigned char *a,
	   	  struct el_form_control *fc)
{
	put_chrs(html_context, " ", 1);
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	html_focusable(html_context, a);
	format.form = fc;
	mem_free_if(format.title);
	dom_html_input_element *input = (dom_html_input_element *)node;
	dom_exception exc;
	dom_string *title_value = NULL;
	
	exc = dom_html_element_get_title((dom_html_element *)node, &title_value);
	if (DOM_NO_ERR == exc && title_value) {
		format.title = memacpy(dom_string_data(title_value), dom_string_byte_length(title_value));
		dom_string_unref(title_value);
	}
	
	//format.title = get_attr_val(a, "title", html_context->doc_cp);
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
			unsigned char *al = NULL;
			dom_string *src_value = NULL;

			mem_free_set(&format.image, NULL);

			exc = dom_html_input_element_get_src(input, &src_value);
			if (DOM_NO_ERR == exc && src_value) {
				al = memacpy(dom_string_data(src_value), dom_string_byte_length(src_value));
				dom_string_unref(src_value);
			}

			//al = get_url_val(a, "src", html_context->doc_cp);
#if 0
			if (!al) {
				al = get_url_val(a, "dynsrc",
				                 html_context->doc_cp);
			}
#endif
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
tags_html_input(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	unsigned char *al = NULL;
	struct el_form_control *fc;
	bool disabled = false;
	bool readonly = false;
	dom_exception exc;
	dom_html_input_element *input = (dom_html_input_element *)node;
	dom_string *type_value = NULL;
	dom_string *value_value = NULL;
	dom_string *id_value = NULL;
	dom_string *name_value = NULL;
	unsigned int size = 0;
	
	fc = tags_init_form_control(FC_TEXT, a, html_context);
	if (!fc) return;

	exc = dom_html_input_element_get_disabled(input, &disabled);
	if (DOM_NO_ERR == exc) {
		if (disabled) {
			fc->mode = FORM_MODE_DISABLED;
		}
	}

	if (!disabled) {
		exc = dom_html_input_element_get_read_only(input, &readonly);
		if (DOM_NO_ERR == exc) {
			if (readonly) {
				fc->mode = FORM_MODE_READONLY;
			}
		}
	}
	
	exc = dom_html_input_element_get_type(input, &type_value);
	if (DOM_NO_ERR == exc && type_value) {
		al = memacpy(dom_string_data(type_value), dom_string_byte_length(type_value));
		dom_string_unref(type_value);
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

	exc = dom_html_input_element_get_value(input, &value_value);
	if (DOM_NO_ERR == exc && value_value) {
		if (fc->type == FC_HIDDEN) {
			fc->default_value = memacpy(dom_string_data(value_value), dom_string_byte_length(value_value));
		} else if (fc->type != FC_FILE) {
			fc->default_value = memacpy(dom_string_data(value_value), dom_string_byte_length(value_value));			
		}
		dom_string_unref(value_value);
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

	exc = dom_html_element_get_id((dom_html_element *)node, &id_value);
	if (DOM_NO_ERR == exc && id_value) {
		fc->id = memacpy(dom_string_data(id_value), dom_string_byte_length(id_value));
		dom_string_unref(id_value);
	}
	
	//fc->id = get_attr_val(a, "id", cp);

	exc = dom_html_input_element_get_name(input, &name_value);
	if (DOM_NO_ERR == exc && name_value) {
		fc->name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
		dom_string_unref(name_value);
	}
	//fc->name = get_attr_val(a, "name", cp);

	exc = dom_html_input_element_get_size(input, &size);
	if (DOM_NO_ERR == exc) {
		fc->size = size;
	}
	//fc->size = get_num(a, "size", cp);
	if (fc->size <= 0)
		fc->size = html_context->options->default_form_input_size;
	fc->size++;
	if (fc->size > html_context->options->document_width)
		fc->size = html_context->options->document_width;

	(void)dom_html_input_element_get_max_length(input, &fc->maxlength);

	//fc->maxlength = get_num(a, "maxlength", cp);
	if (fc->maxlength == -1) fc->maxlength = INT_MAX;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) {
		bool checked = false;
		exc = dom_html_input_element_get_checked(input, &checked);
		if (DOM_NO_ERR == exc) {
			//fc->default_state = has_attr(a, "checked", cp);
			fc->default_state = checked;
		}
	}
	if (fc->type == FC_IMAGE) {
		dom_string *alt_value = NULL;

		exc = dom_html_input_element_get_alt(input, &alt_value);
		if (DOM_NO_ERR == exc && alt_value) {
			fc->alt = memacpy(dom_string_data(alt_value), dom_string_byte_length(alt_value));
			dom_string_unref(alt_value);
		}
//		fc->alt = get_attr_val(a, "alt", cp);
	}

	if (fc->type != FC_HIDDEN) {
		tags_html_input_format(html_context, node, a, fc);
	}

	html_context->special_f(html_context, SP_CONTROL, fc);
}

void
tags_html_input_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ins(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ins_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_isindex(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_isindex_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_kbd(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_kbd_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_keygen(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_keygen_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_label(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_label_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_legend(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_legend_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_li(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_html_li_element *node = (dom_html_li_element *)no;
	int t = par_format.flags & P_LISTMASK;

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
	} else if (!par_format.list_number) {
		if (t == P_O) /* Print U+25E6 WHITE BULLET. */
			put_chrs(html_context, "&#9702;", 7);
		else if (t == P_SQUARE) /* Print U+25AA BLACK SMALL SQUARE. */
			put_chrs(html_context, "&#9642;", 7);
		else /* Print U+2022 BULLET. */
			put_chrs(html_context, "&#8226;", 7);
		put_chrs(html_context, "&nbsp;", 6);
		par_format.leftmargin += 2;
		par_format.align = ALIGN_LEFT;

	} else {
		dom_long s = -1;
		unsigned char c = 0;
		int nlen;
		int t = par_format.flags & P_LISTMASK;
		struct string n;

		(void)dom_html_li_element_get_value(node, &s);

		if (!init_string(&n)) return;

		if (s != -1) par_format.list_number = s;

		if (t == P_ALPHA || t == P_alpha) {
			unsigned char n0;

			put_chrs(html_context, "&nbsp;", 6);
			c = 1;
			n0 = par_format.list_number
			       ? (par_format.list_number - 1) % 26
			         + (t == P_ALPHA ? 'A' : 'a')
			       : 0;
			if (n0) add_char_to_string(&n, n0);

		} else if (t == P_ROMAN || t == P_roman) {
			roman(&n, par_format.list_number);
			if (t == P_ROMAN) {
				unsigned char *x;

				for (x = n.source; *x; x++) *x = c_toupper(*x);
			}

		} else {
			unsigned char n0[64];
			if (par_format.list_number < 10) {
				put_chrs(html_context, "&nbsp;", 6);
				c = 1;
			}

			ulongcat(n0, NULL, par_format.list_number, (sizeof(n) - 1), 0);
			add_to_string(&n, n0);
		}

		nlen = n.length;
		put_chrs(html_context, n.source, nlen);
		put_chrs(html_context, ".&nbsp;", 7);
		par_format.leftmargin += nlen + c + 2;
		par_format.align = ALIGN_LEFT;
		done_string(&n);

		{
			struct html_element *element;

			element = search_html_stack(html_context, DOM_HTML_ELEMENT_TYPE_OL);
			if (element)
				element->parattr.list_number = par_format.list_number + 1;
		}

		par_format.list_number = 0;
	}

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = 2;
	html_context->was_li = 1;
}

void
tags_html_li_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_link_parse(struct source_renderer *renderer, dom_node *node, unsigned char *a,
                struct hlink *link)
{
	//struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_html_link_element *link_element = (dom_html_link_element *)node;
	dom_string *href_value = NULL;
	dom_string *lang_value = NULL;
	dom_string *hreflang_value = NULL;
	dom_string *title_value = NULL;
	dom_string *type_value = NULL;
	dom_string *media_value = NULL;
	dom_string *rel_value = NULL;
	dom_string *rev_value = NULL;

	int i;

	assert(/*a &&*/ link);
	memset(link, 0, sizeof(*link));

	exc = dom_html_link_element_get_href(link_element, &href_value);
	if (DOM_NO_ERR == exc && href_value) {
		link->href = memacpy(dom_string_data(href_value), dom_string_byte_length(href_value));
		dom_string_unref(href_value);
	}
	
//	link->href = get_url_val(a, "href", html_context->doc_cp);
	if (!link->href) return 0;

	exc = dom_html_element_get_lang((dom_html_element *)node, &lang_value);
	if (DOM_NO_ERR == exc && lang_value) {
		link->lang = memacpy(dom_string_data(lang_value), dom_string_byte_length(lang_value));
		dom_string_unref(lang_value);
	}
	//link->lang = get_attr_val(a, "lang", html_context->doc_cp);

	exc = dom_html_link_element_get_hreflang(link_element, &hreflang_value);
	if (DOM_NO_ERR == exc && hreflang_value) {
		link->hreflang = memacpy(dom_string_data(hreflang_value), dom_string_byte_length(hreflang_value));
		dom_string_unref(hreflang_value);
	}

//	link->hreflang = get_attr_val(a, "hreflang", html_context->doc_cp);

	exc = dom_html_element_get_title((dom_html_element *)node, &title_value);
	if (DOM_NO_ERR == exc && title_value) {
		link->title = memacpy(dom_string_data(title_value), dom_string_byte_length(title_value));
		dom_string_unref(title_value);
	}

//	link->title = get_attr_val(a, "title", html_context->doc_cp);
	exc = dom_html_link_element_get_type(link_element, &type_value);
	if (DOM_NO_ERR == exc && type_value) {
		link->content_type = memacpy(dom_string_data(type_value), dom_string_byte_length(type_value));
		dom_string_unref(type_value);
	}
	//link->content_type = get_attr_val(a, "type", html_context->doc_cp);

	exc = dom_html_link_element_get_media(link_element, &media_value);
	if (DOM_NO_ERR == exc && media_value) {
		link->media = memacpy(dom_string_data(media_value), dom_string_byte_length(media_value));
		dom_string_unref(media_value);
	}
	//link->media = get_attr_val(a, "media", html_context->doc_cp);

	exc = dom_html_link_element_get_rel(link_element, &rel_value);
	if (DOM_NO_ERR == exc && rel_value) {
		link->name = memacpy(dom_string_data(rel_value), dom_string_byte_length(rel_value));
		dom_string_unref(rel_value);
	}
	//link->name = get_attr_val(a, "rel", html_context->doc_cp);
	if (link->name) {
		link->direction = LD_REL;
	} else {
		exc = dom_html_link_element_get_rev(link_element, &rev_value);
		if (DOM_NO_ERR == exc && rev_value) {
			link->name = memacpy(dom_string_data(rev_value), dom_string_byte_length(rev_value));
			dom_string_unref(rev_value);
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
tags_html_link(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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

	html_focusable(html_context, a);

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
tags_html_link_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_main(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_main_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_map(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_map_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_mark(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_mark_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menu(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menu_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menuitem(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_menuitem_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_meta(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	/* html_handle_body_meta() does all the work. */
}

void
tags_html_meta_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_meter(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_meter_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_nav(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_nav_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

//void
//tags_html_noframes(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_noscript(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_noscript_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_object(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_object_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ol(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_html_olist_element *node = (dom_html_olist_element *)no;
	dom_exception exc;
	dom_string *type_value;
	unsigned char *al;
	dom_long st = 1;

	par_format.list_level++;
	exc = dom_html_olist_element_get_start(node, &st);
	if (DOM_NO_ERR == exc) {
		if (st == -1) st = 1;
	}
	par_format.list_number = st;
	par_format.flags = P_NUMBER;

	exc = dom_html_olist_element_get_type(node, &type_value);
	if (DOM_NO_ERR == exc) {
		if (type_value) {
			al = memacpy(dom_string_data(type_value), dom_string_byte_length(type_value));
			dom_string_unref(type_value);
	
			if (al) {
				if (*al && !al[1]) {
					if (*al == '1') par_format.flags = P_NUMBER;
					else if (*al == 'a') par_format.flags = P_alpha;
					else if (*al == 'A') par_format.flags = P_ALPHA;
					else if (*al == 'r') par_format.flags = P_roman;
					else if (*al == 'R') par_format.flags = P_ROMAN;
					else if (*al == 'i') par_format.flags = P_roman;
					else if (*al == 'I') par_format.flags = P_ROMAN;
				}
				mem_free(al);
			}
		}
	}

	par_format.leftmargin += (par_format.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = ALIGN_LEFT;
	html_top->type = ELEMENT_DONT_KILL;
}

void
tags_html_ol_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_optgroup(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_optgroup_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_option(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_option_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_output(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_output_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_p(struct source_renderer *renderer, dom_node *node, unsigned char *a,
       unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_html_paragraph_element *element = (dom_html_paragraph_element *)node;
	dom_string *align_value = NULL;
	int_lower_bound(&par_format.leftmargin, html_context->margin);
	int_lower_bound(&par_format.rightmargin, html_context->margin);
	/*par_format.align = ALIGN_LEFT;*/

	exc = dom_html_paragraph_element_get_align(element, &align_value);
	if (DOM_NO_ERR == exc) {
		if (align_value) {
			unsigned char *al = memacpy(dom_string_data(align_value), dom_string_byte_length(align_value));

			dom_string_unref(align_value);
			tags_html_linebrk(renderer, al);
		}
	}
}

void
tags_html_p_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_param(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_param_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_picture(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_picture_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_pre(struct source_renderer *renderer, dom_node *node, unsigned char *a,
         unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	format.style.attr |= AT_PREFORMATTED;
	par_format.leftmargin = (par_format.leftmargin > 1);
	par_format.rightmargin = 0;
}

void
tags_html_pre_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_progress(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_progress_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

/* TODO: Add more languages.
 * Entities can be used in these strings.  */
static unsigned char *quote_char[2] = { "\"", "'" };

void
tags_html_q(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_q_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
tags_html_rp(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_rp_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_rt(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_rt_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ruby(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ruby_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_s(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_s_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_samp(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_samp_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_script(struct source_renderer *renderer, dom_node *no, unsigned char *a,
            unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_string *type_value = NULL;
	dom_string *src_value = NULL;
	dom_html_script_element *node = (dom_html_script_element *)no;
#ifdef CONFIG_ECMASCRIPT
	/* TODO: <noscript> processing. Well, same considerations apply as to
	 * CSS property display: none processing. */
	/* TODO: Charsets for external scripts. */
	unsigned char *type = NULL, *src = NULL;
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
	exc = dom_html_script_element_get_type(node, &type_value);
	if (DOM_NO_ERR == exc && type_value) {
		type = memacpy(dom_string_data(type_value), dom_string_byte_length(type_value));
		dom_string_unref(type_value);
	}
	
	if (type) {
		unsigned char *pos = type;

		if (!c_strncasecmp(type, "text/", 5)) {
			pos += 5;

		} else if (!c_strncasecmp(type, "application/", 12)) {
			pos += 12;

		} else {
			mem_free(type);
not_processed:
			/* Permit nested scripts and retreat. */
			html_top->invisible++;
			return;
		}

		if (!c_strncasecmp(pos, "javascript", 10)) {
			int len = strlen(pos);

			if (len > 10 && !isdigit(pos[10])) {
				mem_free(type);
				goto not_processed;
			}

		} else if (c_strcasecmp(pos, "ecmascript")
		    && c_strcasecmp(pos, "jscript")
		    && c_strcasecmp(pos, "livescript")
		    && c_strcasecmp(pos, "x-javascript")
		    && c_strcasecmp(pos, "x-ecmascript")) {
			mem_free(type);
			goto not_processed;
		}

		mem_free(type);
	}

	if (html_context->part->document) {
		exc = dom_html_script_element_get_src(node, &src_value);
	
		if (DOM_NO_ERR == exc && src_value) {
			src = memacpy(dom_string_data(src_value), dom_string_byte_length(src_value));
			dom_string_unref(src_value);
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
tags_html_script_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	renderer->html_context->skip_html = 0;
}

void
tags_html_section(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_section_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

static void
do_tags_html_select_multiple(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_string *name_value = NULL;

	exc = dom_html_select_element_get_name((dom_html_select_element *)node, &name_value);
	if (DOM_NO_ERR == exc && name_value) {
		bool disabled = false;
		unsigned char *al = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
		dom_string_unref(name_value);

		if (!al) return;
	
	//unsigned char *al = get_attr_val(a, "name", html_context->doc_cp);

	//if (!al) return;
		html_focusable(html_context, a);
		html_top->type = ELEMENT_DONT_KILL;
		mem_free_set(&format.select, al);

		exc = dom_html_select_element_get_disabled((dom_html_select_element *)node, &disabled);
		if (DOM_NO_ERR == exc) {
			format.select_disabled = disabled ? FORM_MODE_DISABLED : FORM_MODE_NORMAL;
		}
	//format.select_disabled = has_attr(a, "disabled", html_context->doc_cp)
	//                         ? FORM_MODE_DISABLED
	//                         : FORM_MODE_NORMAL;
	}
}

static struct list_menu lnk_menu;

static void
do_tags_html_select(struct source_renderer *renderer, dom_node *node, unsigned char *a,
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
	dom_exception exc;
	dom_html_options_collection *options_collection = NULL;
	dom_string *id_value = NULL;
	dom_string *name_value = NULL;
	
	html_focusable(html_context, a);
	init_menu(&lnk_menu);

#if 0
se:
        en = html;

see:
        html = en;
	while (html < eof && *html != '<') html++;

	if (html >= eof) {

abort:
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
//		*end = en;
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
#endif
	exc = dom_html_select_element_get_options((dom_html_select_element *)node, &options_collection);
	if (DOM_NO_ERR == exc && options_collection) {
		int len = 0, i;
		exc = dom_html_options_collection_get_length(options_collection, &len);
		order = 0;
		for (i = 0; i < len; ++i) {
			dom_node *option_node = NULL;

			exc = dom_html_options_collection_item(options_collection, i, &option_node);
			if (DOM_NO_ERR == exc && option_node) {
				dom_html_element_type tag = 0;

				exc = dom_html_element_get_tag_type((dom_html_element *)option_node, &tag);
				if (DOM_HTML_ELEMENT_TYPE_OPTION == tag) {
					bool disabled = false;
					bool selected = false;
					dom_string *value_value = NULL;
					dom_string *label_value = NULL;
					dom_string *text_value = NULL;
					unsigned char *value = NULL;
					unsigned char *label = NULL;
					add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);

					exc = dom_html_option_element_get_disabled((dom_html_option_element *)option_node, &disabled);

					if (disabled) {
						continue;
					}

					exc = dom_html_option_element_get_selected((dom_html_option_element *)option_node, &selected);

					if (-1 == preselect && selected) {
						preselect = order;
					}
					exc = dom_html_option_element_get_value((dom_html_option_element *)option_node, &value_value);
					if (DOM_NO_ERR == exc && value_value) {
						value = memacpy(dom_string_data(value_value), dom_string_byte_length(value_value));
						dom_string_unref(value_value);

						if (!mem_align_alloc(&values, i, i + 1, 0xFF)) {
							goto abort;
						}
						values[order++] = value;
					}
					exc = dom_html_option_element_get_label((dom_html_option_element *)option_node, &label_value);
					if (DOM_NO_ERR == exc && label_value) {
						label = memacpy(dom_string_data(label_value), dom_string_byte_length(label_value));
//fprintf(stderr, "label=%s\n", label);
						dom_string_unref(label_value);
					}
					if (label) {
						new_menu_item(&lnk_menu, label, order - 1, 0);
					}
					if (!value || !label) {
						init_string(&lbl);
						init_string(&orig_lbl);
						nnmi = !!label;
					}
					exc = dom_html_option_element_get_text((dom_html_option_element *)option_node, &text_value);
					if (DOM_NO_ERR == exc && text_value) {
						add_bytes_to_string(&lbl, dom_string_data(text_value), dom_string_byte_length(text_value));
						add_bytes_to_string(&orig_lbl, dom_string_data(text_value), dom_string_byte_length(text_value));
						dom_string_unref(text_value);
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
				} else if (DOM_HTML_ELEMENT_TYPE_OPTGROUP == tag) {
					dom_string *label_value = NULL;
					unsigned char *label = NULL;

					add_select_item(&lnk_menu, &lbl, &orig_lbl, values, order, nnmi);
					if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;

					exc = dom_html_opt_group_element_get_label((dom_html_opt_group_element *)option_node, &label_value);
					if (DOM_NO_ERR == exc && label_value) {
						label = memacpy(dom_string_data(label_value), dom_string_byte_length(label_value));
						dom_string_unref(label_value);
					}
					if (!label) {
						label = stracpy("");
						if (!label) {
							continue;
						}
					}
					new_menu_item(&lnk_menu, label, -1, 0);
					group = 1;
				}
				dom_node_unref(option_node);
				if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;
			}
		}
		dom_html_options_collection_unref(options_collection);
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
	if (!order) goto abort;

	labels = mem_calloc(order, sizeof(unsigned char *));
	if (!labels) goto abort;

	fc = tags_init_form_control(FC_SELECT, a, html_context);
	if (!fc) {
		mem_free(labels);
		goto abort;
	}
	
	exc = dom_html_element_get_id((dom_html_element *)node, &id_value);
	if (DOM_NO_ERR == exc && id_value) {
		fc->id = memacpy(dom_string_data(id_value), dom_string_byte_length(id_value));
		dom_string_unref(id_value);
	}

	exc = dom_html_select_element_get_name((dom_html_select_element *)node, &name_value);
	if (DOM_NO_ERR == exc && name_value) {
		fc->name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
		dom_string_unref(name_value);
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
	format.form = fc;
	format.style.attr |= AT_BOLD;
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
	return;
abort:
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
//		*end = en;
}

void
tags_html_select(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	bool multiple = false;

	dom_exception exc = dom_html_select_element_get_multiple((dom_html_select_element *)node, &multiple);
	renderer->html_context->skip_select = 1;

	if (DOM_NO_ERR == exc && multiple) {
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
tags_html_select_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	renderer->html_context->skip_select = 0;
}

void
tags_html_small(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_small_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_source(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_string *src_value = NULL;
	unsigned char *url = NULL;
	unsigned char *prefix = "";
	dom_exception exc;
	dom_node *parent;
	/* This just places a link where a video element would be. */

	exc = dom_html_image_element_get_src((dom_html_image_element *)node, &src_value);
	if (DOM_NO_ERR == exc && src_value) {
		url = memacpy(dom_string_data(src_value), dom_string_byte_length(src_value));
		dom_string_unref(src_value);
	}
	
	//url = get_url_val(a, "src", html_context->doc_cp);
	if (!url) return;

	html_focusable(html_context, a);

	exc = dom_node_get_parent_node(node, &parent);
	if (DOM_NO_ERR == exc && parent) {
		dom_html_element_type tag = 0;
		exc = dom_html_element_get_tag_type((dom_html_element *)parent, &tag);

		if (DOM_HTML_ELEMENT_TYPE_AUDIO == tag) {
			prefix = "Audio: ";
		} else if (DOM_HTML_ELEMENT_TYPE_VIDEO == tag) {
			prefix = "Video: ";
		}
		dom_node_unref(parent);
	}
	
	put_link_line(prefix, basename(url), url,
              html_context->options->framename, html_context);

	//html_skip(html_context, a);

	mem_free(url);
}

void
tags_html_source_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_span(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_span_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_strong(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_strong_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_style(struct source_renderer *renderer, dom_node *node, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	html_context->was_style = 1;
	html_context->skip_html = 1;
	//html_skip(html_context, a);	
}

void
tags_html_style_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
                 unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	html_context->was_style = 0;
	html_context->skip_html = 0;
}

void
tags_html_sub(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, "[", 1);	
}

void
tags_html_sub_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, "]", 1);
}

void
tags_html_summary(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_summary_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_sup(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	put_chrs(html_context, "^", 1);	
}

void
tags_html_sup_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_table(struct source_renderer *renderer, dom_node *no, unsigned char *attr,
           unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_html_table_element *node = (dom_html_table_element *)no;
	dom_string *align_value = NULL;
	if (html_context->options->tables
	    && html_context->table_level < HTML_MAX_TABLE_LEVEL) {
		format_table(attr, html, eof, end, html_context);
		ln_break(html_context, 2);

		return;
	}
	par_format.leftmargin = par_format.rightmargin = html_context->margin;
	par_format.align = ALIGN_LEFT;

	exc = dom_html_table_element_get_align(node, &align_value);
	if (DOM_NO_ERR == exc) {
		if (align_value) {
			unsigned char *al = memacpy(dom_string_data(align_value), dom_string_byte_length(align_value));
			dom_string_unref(align_value);
			tags_html_linebrk(renderer, al);
		}
	}
	format.style.attr = 0;
}

void
tags_html_table_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tbody(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tbody_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_td(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/*html_linebrk(html_context, a, html, eof, end);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	format.style.attr &= ~AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
tags_html_td_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_template(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_template_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_textarea(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	struct el_form_control *fc;
//	unsigned char *p, *t_name, *wrap_attr;
//	int t_namelen;
	int cols, rows;
	int i;
	bool readonly = false;
	bool disabled = false;
	dom_exception exc;
	dom_html_text_area_element *textarea = (dom_html_text_area_element *)node;
	dom_string *id_value = NULL;
	dom_string *name_value = NULL;
	dom_string *default_value = NULL;

	html_focusable(html_context, NULL);
#if 0
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
#endif
	fc = tags_init_form_control(FC_TEXTAREA, NULL, html_context);
	if (!fc) return;

	exc = dom_html_text_area_element_get_disabled(textarea, &disabled);
	if (DOM_NO_ERR == exc) {
		if (disabled) {
			fc->mode = FORM_MODE_DISABLED;
		}
	}

	if (!disabled) {
		exc = dom_html_text_area_element_get_read_only(textarea, &readonly);
		if (DOM_NO_ERR == exc) {
			if (readonly) {
				fc->mode = FORM_MODE_READONLY;
			}
		}
	}
	
	exc = dom_html_element_get_id((dom_html_element *)node, &id_value);
	if (DOM_NO_ERR == exc && id_value) {
		fc->id = memacpy(dom_string_data(id_value), dom_string_byte_length(id_value));
		dom_string_unref(id_value);
	}
//	fc->id = get_attr_val(attr, "id", html_context->doc_cp);

	exc = dom_html_text_area_element_get_name(textarea, &name_value);
	if (DOM_NO_ERR == exc && name_value) {
		fc->name = memacpy(dom_string_data(name_value), dom_string_byte_length(name_value));
		dom_string_unref(name_value);
	}
//	fc->name = get_attr_val(attr, "name", html_context->doc_cp);

	exc = dom_html_text_area_element_get_value(textarea, &default_value);
	if (DOM_NO_ERR == exc && default_value) {
		fc->default_value = memacpy(dom_string_data(default_value), dom_string_byte_length(default_value));
		dom_string_unref(default_value);
	}

#if 0
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
#endif
	cols = 0;
	exc = dom_html_text_area_element_get_cols(textarea, &cols);
//	cols = get_num(attr, "cols", html_context->doc_cp);
	if (cols <= 0)
		cols = html_context->options->default_form_input_size;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > html_context->options->document_width)
		cols = html_context->options->document_width;
	fc->cols = cols;

	rows = 0;
	exc = dom_html_text_area_element_get_rows(textarea, &rows);
//	rows = get_num(attr, "rows", html_context->doc_cp);
	if (rows <= 0) rows = 1;
	if (rows > html_context->options->box.height)
		rows = html_context->options->box.height;
	fc->rows = rows;
	html_context->options->needs_height = 1;
#if 0
	
	
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
#endif
	fc->wrap = FORM_WRAP_SOFT;
	fc->maxlength = -1;
	exc = dom_html_input_element_get_max_length((dom_html_input_element *)node, &fc->maxlength);
//	fc->maxlength = get_num(attr, "maxlength", html_context->doc_cp);
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
	html_context->skip_textarea = 1;
	html_context->special_f(html_context, SP_CONTROL, fc);
}

void
tags_html_textarea_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;

	//fprintf(stderr, "tags_html_textarea_close\n");
	html_context->skip_textarea = 0;
}

void
tags_html_tfoot(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tfoot_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_th(struct source_renderer *renderer, dom_node *node, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	/*html_linebrk(html_context, a, html, eof, end);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	format.style.attr |= AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
tags_html_th_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_thead(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_thead_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_time(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_time_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_title(struct source_renderer *renderer, dom_node *node, unsigned char *a,
           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	html_top->invisible = 1;
	html_top->type = ELEMENT_WEAK;
}

void
tags_html_title_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_tr(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *html, unsigned char *eof, unsigned char **end)
{
	dom_exception exc;
	dom_html_table_row_element *node = (dom_html_table_row_element *)no;
	dom_string *align_value = NULL;

	exc = dom_html_table_row_element_get_align(node, &align_value);
	if (DOM_NO_ERR == exc) {
		if (align_value) {
			unsigned char *al = memacpy(dom_string_data(align_value), dom_string_byte_length(align_value));
			dom_string_unref(align_value);
			tags_html_linebrk(renderer, al);
		}
	}
}

void
tags_html_tr_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_track(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_track_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}
//
//void
//tags_html_tt(struct source_renderer *renderer, dom_node *node, unsigned char *a,
//        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
//{
//}

void
tags_html_u(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	format.style.attr |= AT_UNDERLINE;
}

void
tags_html_u_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_ul(struct source_renderer *renderer, dom_node *no, unsigned char *a,
        unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
	struct html_context *html_context = renderer->html_context;
	dom_html_u_list_element *node = (dom_html_u_list_element *)no;
	unsigned char *al = NULL;
	dom_exception exc;
	dom_string *type_value = NULL;

	/* dump_html_stack(html_context); */
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.flags = P_DISC;

	exc = dom_html_u_list_element_get_type(node, &type_value);
	if (DOM_NO_ERR == exc) {
		if (type_value) {
			al = memacpy(dom_string_data(type_value), dom_string_byte_length(type_value));
			dom_string_unref(type_value);
		}
	}
	
	if (al) {
		if (!c_strcasecmp(al, "disc"))
			par_format.flags = P_DISC;
		else if (!c_strcasecmp(al, "circle"))
			par_format.flags = P_O;
		else if (!c_strcasecmp(al, "square"))
			par_format.flags = P_SQUARE;
		mem_free(al);
	}
	par_format.leftmargin += 2 + (par_format.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = ALIGN_LEFT;
	html_top->type = ELEMENT_DONT_KILL;
}

void
tags_html_ul_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_var(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_var_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_video(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_video_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_wbr(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

void
tags_html_wbr_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
          unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
{
}

//void
//tags_html_fixed(struct source_renderer *renderer, dom_node *node, unsigned char *a,
//           unsigned char *xxx3, unsigned char *xxx4, unsigned char **xxx5)
//{
//	format.style.attr |= AT_FIXED;
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
		if (!c_strcasecmp(al, "left")) par_format.align = ALIGN_LEFT;
		else if (!c_strcasecmp(al, "right")) par_format.align = ALIGN_RIGHT;
		else if (!c_strcasecmp(al, "center")) {
			par_format.align = ALIGN_CENTER;
			if (!html_context->table_level)
				par_format.leftmargin = par_format.rightmargin = 0;
		} else if (!c_strcasecmp(al, "justify")) par_format.align = ALIGN_JUSTIFY;
		mem_free(al);
	}
}

static void
tags_html_h(int h, dom_node *node, unsigned char *a,
       enum format_align default_align, struct source_renderer *renderer,
       unsigned char *html, unsigned char *eof, unsigned char **end)
{
	struct html_context *html_context = renderer->html_context;
	dom_exception exc;
	dom_html_heading_element *element = (dom_html_heading_element *)node;
	dom_string *align_value = NULL;
	
	if (!par_format.align) par_format.align = default_align;
	exc = dom_html_heading_element_get_align(element, &align_value);
	if (DOM_NO_ERR == exc) {
		if (align_value) {
			unsigned char *al = memacpy(dom_string_data(align_value), dom_string_byte_length(align_value));

			dom_string_unref(align_value);
			tags_html_linebrk(renderer, al);
		}
	}

	h -= 2;
	if (h < 0) h = 0;

	switch (par_format.align) {
		case ALIGN_LEFT:
			par_format.leftmargin = h * 2;
			par_format.rightmargin = 0;
			break;
		case ALIGN_RIGHT:
			par_format.leftmargin = 0;
			par_format.rightmargin = h * 2;
			break;
		case ALIGN_CENTER:
			par_format.leftmargin = par_format.rightmargin = 0;
			break;
		case ALIGN_JUSTIFY:
			par_format.leftmargin = par_format.rightmargin = h * 2;
			break;
	}
}

//void
//tags_html_xmp(struct source_renderer *renderer, dom_node *node, unsigned char *a,
//         unsigned char *html, unsigned char *eof, unsigned char **end)
//{
//	html_context->was_xmp = 1;
//	html_pre(html_context, a, html, eof, end);
//}

//void
//tags_html_xmp_close(struct source_renderer *renderer, dom_node *node, unsigned char *a,
//               unsigned char *html, unsigned char *eof, unsigned char **end)
//{
//	html_context->was_xmp = 0;
//}



void
tags_html_nop(struct source_renderer *renderer, dom_node *node, unsigned char *a,
               unsigned char *html, unsigned char *eof, unsigned char **end)
{
}

struct element_info2 tags_dom_html_array[] = {
	{NULL, NULL, 0, ET_NON_NESTABLE}, /* _UNKNOWN */
	{tags_html_a, NULL, 0, ET_NON_NESTABLE}, /* A */
	{tags_html_abbr, NULL, 0, ET_NESTABLE}, /* ABBR */
	{tags_html_address, NULL, 2, ET_NESTABLE}, /* ADDRESS */
	{tags_html_applet, NULL, 1, ET_NON_PAIRABLE }, /* APPLET */
	{tags_html_area, tags_html_area_close, 0x0, 0x0}, /* AREA */
	{tags_html_article, tags_html_article_close, 0x0, 0x0}, /* ARTICLE */
	{tags_html_aside, tags_html_aside_close, 0x0, 0x0}, /* ASIDE */
	{tags_html_audio, NULL, 1, ET_NON_PAIRABLE}, /* AUDIO */
	{tags_html_b, NULL, 0, ET_NESTABLE}, /* B */
	{tags_html_base, NULL, 0, ET_NON_PAIRABLE}, /* BASE */
	{tags_html_basefont, NULL, 0, ET_NON_PAIRABLE}, /* BASEFONT */
	{tags_html_bdi, tags_html_bdi_close, 0x0, 0x0}, /* BDI */
	{tags_html_bdo, tags_html_bdo_close, 0x0, 0x0}, /* BDO */
	{tags_html_blockquote, NULL, 2, ET_NESTABLE}, /* BLOCKQUOTE */
	{tags_html_body, NULL, 0, ET_NESTABLE}, /* BODY */
	{tags_html_br, NULL, 1, ET_NON_PAIRABLE}, /* BR */
	{tags_html_button, NULL, 0, ET_NESTABLE}, /* BUTTON */
	{tags_html_canvas, tags_html_canvas_close, 0x0, 0x0}, /* CANVAS */
	{tags_html_caption, NULL, 1, ET_NESTABLE}, /* CAPTION */
	{tags_html_center, NULL, 1, ET_NESTABLE}, /* CENTER */
	{tags_html_cite, tags_html_cite_close, 0x0, 0x0}, /* CITE */
	{tags_html_code, NULL, 0, ET_NESTABLE}, /* CODE */
	{tags_html_col, tags_html_col_close, 0x0, 0x0}, /* COL */
	{tags_html_colgroup, tags_html_colgroup_close, 0x0, 0x0}, /* COLGROUP */
	{tags_html_data, tags_html_data_close, 0x0, 0x0}, /* DATA */
	{tags_html_datalist, tags_html_datalist_close, 0x0, 0x0}, /* DATALIST */
	{tags_html_dd, NULL, 1, ET_NON_PAIRABLE}, /* DD */
	{tags_html_del, tags_html_del_close, 0x0, 0x0}, /* DEL */
	{tags_html_details, tags_html_details_close, 0x0, 0x0}, /* DETAILS */
	{tags_html_dfn, NULL, 0, ET_NESTABLE}, /* DFN */
	{tags_html_dialog, tags_html_dialog_close, 0x0, 0x0}, /* DIALOG */
	{tags_html_dir, NULL, 2, ET_NESTABLE}, /* DIR */
	{tags_html_div, NULL, 1, ET_NESTABLE}, /* DIV */
	{tags_html_dl, NULL, 2, ET_NESTABLE}, /* DL */
	{tags_html_dt, NULL, 1, ET_NON_PAIRABLE}, /* DT */
	{tags_html_em, NULL, 0, ET_NESTABLE}, /* EM */
	{tags_html_embed, NULL, 0, ET_NON_PAIRABLE}, /* EMBED */
	{tags_html_fieldset, tags_html_fieldset_close, 0x0, 0x0}, /* FIELDSET */
	{tags_html_figcaption, tags_html_figcaption_close, 0x0, 0x0}, /* FIGCAPTION */
	{tags_html_figure, tags_html_figure_close, 0x0, 0x0}, /* FIGURE */
	{tags_html_font, NULL, 0, ET_NESTABLE}, /* FONT */
	{tags_html_footer, tags_html_footer_close, 0x0, 0x0}, /* FOOTER */
	{tags_html_form, NULL, 1, ET_NESTABLE}, /* FORM */
	{tags_html_frame, NULL, 1, ET_NON_PAIRABLE}, /* FRAME */
	{tags_html_frameset, NULL, 1, ET_NESTABLE}, /* FRAMESET */
	{tags_html_h1, NULL, 2, ET_NON_NESTABLE}, /* H1 */
	{tags_html_h2, NULL, 2, ET_NON_NESTABLE}, /* H2 */
	{tags_html_h3, NULL, 2, ET_NON_NESTABLE}, /* H3 */
	{tags_html_h4, NULL, 2, ET_NON_NESTABLE}, /* H4 */
	{tags_html_h5, NULL, 2, ET_NON_NESTABLE}, /* H5 */
	{tags_html_h6, NULL, 2, ET_NON_NESTABLE}, /* H6 */
	{tags_html_head, NULL, 0, ET_NESTABLE}, /* HEAD */
	{tags_html_header, tags_html_header_close, 0x0, 0x0}, /* HEADER */
	{tags_html_hgroup, tags_html_hgroup_close, 0x0, 0x0}, /* HGROUP */
	{tags_html_hr, NULL, 2, ET_NON_PAIRABLE}, /* HR */
	{tags_html_html, tags_html_html_close, 0, ET_NESTABLE}, /* HTML */
	{tags_html_i, NULL, 0, ET_NESTABLE}, /* I */
	{tags_html_iframe, NULL, 1, ET_NON_PAIRABLE}, /* IFRAME */
	{tags_html_img, NULL, 0, ET_NON_PAIRABLE}, /* IMG */
	{tags_html_input, NULL, 0, ET_NON_PAIRABLE}, /* INPUT */
	{tags_html_ins, tags_html_ins_close, 0x0, 0x0}, /* INS */
	{tags_html_isindex, tags_html_isindex_close, 0x0, 0x0}, /* ISINDEX */
	{tags_html_kbd, tags_html_kbd_close, 0x0, 0x0}, /* KBD */
	{tags_html_keygen, tags_html_keygen_close, 0x0, 0x0}, /* KEYGEN */
	{tags_html_label, tags_html_label_close, 0x0, 0x0}, /* LABEL */
	{tags_html_legend, tags_html_legend_close, 0x0, 0x0}, /* LEGEND */
	{tags_html_li, NULL, 1, ET_LI}, /* LI */
	{tags_html_link, NULL, 1, ET_NON_PAIRABLE}, /* LINK */
	{tags_html_main, tags_html_main_close, 0x0, 0x0}, /* MAIN */
	{tags_html_map, tags_html_map_close, 0x0, 0x0}, /* MAP */
	{tags_html_mark, tags_html_mark_close, 0x0, 0x0}, /* MARK */
	{tags_html_menu, NULL, 2, ET_NESTABLE}, /* MENU */
	{tags_html_menuitem, tags_html_menuitem_close, 0x0, 0x0}, /* MENUITEM */
	{tags_html_meta, NULL, 0, ET_NON_PAIRABLE}, /* META */
	{tags_html_meter, tags_html_meter_close, 0x0, 0x0}, /* METER */
	{tags_html_nav, tags_html_nav_close, 0x0, 0x0}, /* NAV */
	{tags_html_noscript, NULL, 0, ET_NESTABLE}, /* NOSCRIPT */
	{tags_html_object, NULL, 1, ET_NON_PAIRABLE}, /* OBJECT */
	{tags_html_ol, NULL, 2, ET_NESTABLE}, /* OL */
	{tags_html_optgroup, tags_html_optgroup_close, 0x0, 0x0}, /* OPTGROUP */
	{tags_html_option, NULL, 1, ET_NON_PAIRABLE}, /* OPTION */
	{tags_html_output, tags_html_output_close, 0x0, 0x0}, /* OUTPUT */
	{tags_html_p, NULL, 2, ET_NON_NESTABLE}, /* P */
	{tags_html_param, tags_html_param_close, 0x0, 0x0}, /* PARAM */
	{tags_html_picture, tags_html_picture_close, 0x0, 0x0}, /* PICTURE */
	{tags_html_pre, NULL, 2, ET_NESTABLE}, /* PRE */
	{tags_html_progress, tags_html_progress_close, 0x0, 0x0}, /* PROGRESS */
	{tags_html_q, tags_html_q_close, 0, ET_NESTABLE}, /* Q */
	{tags_html_rp, tags_html_rp_close, 0x0, 0x0}, /* RP */
	{tags_html_rt, tags_html_rt_close, 0x0, 0x0}, /* RT */
	{tags_html_ruby, tags_html_ruby_close, 0x0, 0x0}, /* RUBY */
	{tags_html_s, NULL, 0, ET_NESTABLE}, /* S */
	{tags_html_samp, tags_html_samp_close, 0x0, 0x0}, /* SAMP */
	{tags_html_script, NULL, 0, ET_NESTABLE}, /* SCRIPT */
	{tags_html_section, tags_html_section_close, 0x0, 0x0}, /* SECTION */
	{tags_html_select, tags_html_select_close, 0, ET_NESTABLE}, /* SELECT */
	{tags_html_small, tags_html_small_close, 0x0, 0x0}, /* SMALL */
	{tags_html_source, NULL, 1, ET_NON_PAIRABLE}, /* SOURCE */
	{tags_html_span, NULL, 0, ET_NESTABLE}, /* SPAN */
	{tags_html_strong, NULL, 0, ET_NESTABLE}, /* STRONG */
	{tags_html_style, tags_html_style_close, 0, ET_NESTABLE}, /* STYLE */
	{tags_html_sub, tags_html_sub_close, 0, ET_NESTABLE}, /* SUB */
	{tags_html_summary, tags_html_summary_close, 0x0, 0x0}, /* SUMMARY */
	{tags_html_sup, NULL, 0, ET_NESTABLE}, /* SUP */
	{tags_html_table, NULL, 2, ET_NESTABLE}, /* TABLE */
	{tags_html_tbody, tags_html_tbody_close, 0x0, 0x0}, /* TBODY */
	{tags_html_td, NULL, 0, ET_NESTABLE}, /* TD */
	{tags_html_template, tags_html_template_close, 0x0, 0x0}, /* TEMPLATE */
	{tags_html_textarea, tags_html_textarea_close, 0, 0x0}, /* TEXTAREA */
	{tags_html_tfoot, tags_html_tfoot_close, 0x0, 0x0}, /* TFOOT */
	{tags_html_th, NULL, 0, ET_NESTABLE}, /* TH */
	{tags_html_thead, tags_html_thead_close, 0x0, 0x0}, /* THEAD */
	{tags_html_time, tags_html_time_close, 0x0, 0x0}, /* TIME */
	{tags_html_title, NULL, 0, ET_NESTABLE}, /* TITLE */
	{tags_html_tr, NULL, 1, ET_NESTABLE}, /* TR */
	{tags_html_track, tags_html_track_close, 0x0, 0x0}, /* TRACK */
	{tags_html_u, NULL, 0, ET_NESTABLE}, /* U */
	{tags_html_ul, NULL, 2, ET_NESTABLE}, /* UL */
	{tags_html_var, tags_html_var_close, 0x0, 0x0}, /* VAR */
	{tags_html_video, NULL, 1, ET_NON_PAIRABLE}, /* VIDEO */
	{tags_html_wbr, tags_html_wbr_close, 0x0, 0x0}, /* WBR */
};
