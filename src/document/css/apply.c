/** CSS style applier
 * @file
 *
 * @todo TODO: A way to disable CSS completely, PLUS a way to stop
 * various property groups from taking effect. (Ie. way to turn out
 * effect of 'display: none' or aligning or colors but keeping all the
 * others.) --pasky */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/stylesheet.h"
#include "document/format.h"
#include "document/html/parser/parse.h"
#include "document/options.h"
#include "util/align.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* XXX: Some strange dependency makes it necessary to this include last. */
#include "document/html/internal.h"


typedef void (*css_applier_T)(struct html_context *html_context,
			      struct html_element *element,
			      struct css_property *prop);

static void
css_apply_color(struct html_context *html_context, struct html_element *element,
		struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_COLOR);

	if (use_document_fg_colors(html_context->options))
		element->attr.style.color.foreground = prop->value.color;
}

static void
css_apply_background_color(struct html_context *html_context,
			   struct html_element *element,
			   struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_COLOR);

	if (use_document_bg_colors(html_context->options))
		element->attr.style.color.background = prop->value.color;
}

static void
css_apply_display(struct html_context *html_context, struct html_element *element,
		  struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_DISPLAY);

	switch (prop->value.display) {
		case CSS_DISP_INLINE:
			element->linebreak = 0;
			break;
		case CSS_DISP_BLOCK:
			/* 1 or 2, that is the question. I went for 2 since it
			 * gives a more "blocky" feel and it's more common.
			 * YMMV. */
			element->linebreak = 2;
			break;
		case CSS_DISP_NONE:
			if (!html_context->options->css_ignore_display_none)
				element->invisible = 2;
			break;
		default:
			INTERNAL("Bad prop->value.display %d", prop->value.display);
			break;
	}
}

static void
css_apply_font_attribute(struct html_context *html_context,
			 struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_FONT_ATTRIBUTE);
	element->attr.style.attr |= prop->value.font_attribute.add;
	element->attr.style.attr &= ~prop->value.font_attribute.rem;
}

static void
css_apply_list_style(struct html_context *html_context,
                     struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_LIST_STYLE);

	element->parattr.list_number = (prop->value.list_style > CSS_LIST_ORDINAL);
	switch (prop->value.list_style) {
	case CSS_LIST_NONE: element->parattr.flags = P_NO_BULLET; break;
	case CSS_LIST_DISC: element->parattr.flags = P_DISC; break;
	case CSS_LIST_CIRCLE: element->parattr.flags = P_O; break;
	case CSS_LIST_SQUARE: element->parattr.flags = P_SQUARE; break;
	case CSS_LIST_DECIMAL: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_DECIMAL_LEADING_ZERO: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_LOWER_ROMAN: element->parattr.flags = P_roman; break;
	case CSS_LIST_UPPER_ROMAN: element->parattr.flags = P_ROMAN; break;
	case CSS_LIST_LOWER_ALPHA: element->parattr.flags = P_alpha; break;
	case CSS_LIST_UPPER_ALPHA: element->parattr.flags = P_ALPHA; break;
	case CSS_LIST_LOWER_GREEK: element->parattr.flags = P_roman; break;
	case CSS_LIST_LOWER_LATIN: element->parattr.flags = P_alpha; break;
	case CSS_LIST_UPPER_LATIN: element->parattr.flags = P_ALPHA; break;
	case CSS_LIST_HEBREW: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_ARMENIAN: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_GEORGIAN: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_CJK_IDEOGRAPHIC: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_HIRAGANA: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_KATAKANA: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_HIRAGANA_IROHA: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_KATAKANA_IROHA: element->parattr.flags = P_NUMBER; break;
	case CSS_LIST_ORDINAL: assert(0); break;
	}
}

/** @bug FIXME: Because the current CSS doesn't provide reasonable
 * defaults for each HTML element this applier will cause bad
 * rendering of @<pre> tags. */
static void
css_apply_text_align(struct html_context *html_context,
		     struct html_element *element, struct css_property *prop)
{
	assert(prop->value_type == CSS_VT_TEXT_ALIGN);
	element->parattr.align = prop->value.text_align;
}

/*! XXX: Sort like the css_property_type */
static const css_applier_T css_appliers[CSS_PT_LAST] = {
	/* CSS_PT_NONE */		NULL,
	/* CSS_PT_BACKGROUND */		css_apply_background_color,
	/* CSS_PT_BACKGROUND_COLOR */	css_apply_background_color,
	/* CSS_PT_COLOR */		css_apply_color,
	/* CSS_PT_DISPLAY */		css_apply_display,
	/* CSS_PT_FONT_STYLE */		css_apply_font_attribute,
	/* CSS_PT_FONT_WEIGHT */	css_apply_font_attribute,
	/* CSS_PT_LIST_STYLE */		css_apply_list_style,
	/* CSS_PT_LIST_STYLE_TYPE */	css_apply_list_style,
	/* CSS_PT_TEXT_ALIGN */		css_apply_text_align,
	/* CSS_PT_TEXT_DECORATION */	css_apply_font_attribute,
	/* CSS_PT_WHITE_SPACE */	css_apply_font_attribute,
};

/** This looks for a match in list of selectors. */
static void
examine_element(struct html_context *html_context, struct css_selector *base,
		enum css_selector_type seltype, enum css_selector_relation rel,
		struct css_selector_set *selectors,
		struct html_element *element)
{
	struct css_selector *selector;

#ifdef DEBUG_CSS
	/* Cannot use list_empty() inside the arglist of DBG() because
	 * GCC 4.1 "warning: operation on `errfile' may be undefined"
	 * breaks the build with -Werror.  */
	int dbg_has_leaves, dbg_has_properties;

 	DBG("examine_element(%p, %s, %d, %d, %p, %.*s);", html_context, base->name, seltype, rel, selectors, element->namelen, element->name);
#define dbginfo(sel, type_, base) \
	dbg_has_leaves = !css_selector_set_empty(&sel->leaves), \
	dbg_has_properties = !list_empty(sel->properties), \
	DBG("Matched selector %s (rel %d type %d [m%d])! Children %p !!%d, props !!%d", sel->name, sel->relation, sel->type, sel->type == type_, &sel->leaves, dbg_has_leaves, dbg_has_properties)
#else
#define dbginfo(sel, type, base)
#endif

#define process_found_selector(sel, type, base) \
	if (selector) { \
		dbginfo(sel, type, base); \
		merge_css_selectors(base, sel); \
		/* Ancestor matches? */ \
		if (sel->leaves.may_contain_rel_ancestor_or_parent \
		    && (LIST_OF(struct html_element) *) element->next \
		     != &html_context->stack) { \
			struct html_element *ancestor; \
			/* This is less effective than doing reverse iterations,
			 * first over sel->leaves and then over the HTML stack,
			 * which shines in the most common case where there are
			 * no CSR_ANCESTOR selector leaves. However we would
			 * have to duplicate the whole examine_element(), so if
			 * profiles won't show it really costs... */ \
			for (ancestor = element->next; \
			     (LIST_OF(struct html_element) *) ancestor	\
			      != &html_context->stack;\
			     ancestor = ancestor->next) \
				examine_element(html_context, base, \
						CST_ELEMENT, CSR_ANCESTOR, \
						&sel->leaves, ancestor); \
			examine_element(html_context, base, \
			                CST_ELEMENT, CSR_PARENT, \
			                &sel->leaves, element->next); \
		} \
		/* More specific matches? */ \
		examine_element(html_context, base, type + 1, \
		                CSR_SPECIFITY, \
		                &sel->leaves, element); \
	}

	if (seltype <= CST_ELEMENT && element->namelen) {
		selector = find_css_selector(selectors, CST_ELEMENT, rel,
		                             "*", 1);
		process_found_selector(selector, CST_ELEMENT, base);

		selector = find_css_selector(selectors, CST_ELEMENT, rel,
		                             element->name, element->namelen);
		process_found_selector(selector, CST_ELEMENT, base);
	}

	if (!element->options)
		return;

	/* TODO: More pseudo-classess. --pasky */
	if (element->pseudo_class & ELEMENT_LINK) {
		selector = find_css_selector(selectors, CST_PSEUDO, rel, "link", -1);
		process_found_selector(selector, CST_PSEUDO, base);
	}
	if (element->pseudo_class & ELEMENT_VISITED) {
		selector = find_css_selector(selectors, CST_PSEUDO, rel, "visited", -1);
		process_found_selector(selector, CST_PSEUDO, base);
	}

	if (element->attr.class_ && seltype <= CST_CLASS) {
		const unsigned char *class_ = element->attr.class_;

		for (;;) {
			const unsigned char *begin;

			while (*class_ == ' ') ++class_;
			if (*class_ == '\0') break;
			begin = class_;
			while (*class_ != ' ' && *class_ != '\0') ++class_;

			selector = find_css_selector(selectors, CST_CLASS, rel,
						     begin, class_ - begin);
			process_found_selector(selector, CST_CLASS, base);
		}
	}

	if (element->attr.id && seltype <= CST_ID) {
		selector = find_css_selector(selectors, CST_ID, rel, element->attr.id, -1);
		process_found_selector(selector, CST_ID, base);
	}

#undef process_found_selector
#undef dbginfo
}

struct css_selector *
get_css_selector_for_element(struct html_context *html_context,
			     struct html_element *element,
			     struct css_stylesheet *css,
			     LIST_OF(struct html_element) *html_stack)
{
	unsigned char *code;
	struct css_selector *selector;

	assert(element && element->options && css);

	selector = init_css_selector(NULL, CST_ELEMENT, CSR_ROOT, NULL, 0);
	if (!selector)
		return NULL;

#ifdef DEBUG_CSS
	DBG("Applying to element %.*s...", element->namelen, element->name);
#endif

	examine_element(html_context, selector, CST_ELEMENT, CSR_ROOT,
	                &css->selectors, element);

#ifdef DEBUG_CSS
	DBG("Element %.*s applied.", element->namelen, element->name);
#endif

	code = get_attr_val(element->options, "style", html_context->doc_cp);
	if (code) {
		struct css_selector *stylesel;
		struct scanner scanner;

		stylesel = init_css_selector(NULL, CST_ELEMENT, CSR_ROOT, NULL, 0);

		if (stylesel) {
			init_scanner(&scanner, &css_scanner_info, code, NULL);
			css_parse_properties(&stylesel->properties, &scanner);
			merge_css_selectors(selector, stylesel);
			done_css_selector(stylesel);
		}
		mem_free(code);
	}

	return selector;
}

void
apply_css_selector_style(struct html_context *html_context,
			 struct html_element *element,
			 struct css_selector *selector)
{
	struct css_property *property;

	foreach (property, selector->properties) {
		assert(property->type < CSS_PT_LAST);
		/* We don't assert general prop->value_type here because I
		 * don't want hinder properties' ability to potentially make
		 * use of multiple value types. */
		assert(css_appliers[property->type]);
		css_appliers[property->type](html_context, element, property);
	}
}

void
css_apply(struct html_context *html_context, struct html_element *element,
	  struct css_stylesheet *css, LIST_OF(struct html_element) *html_stack)
{
	struct css_selector *selector;

	selector = get_css_selector_for_element(html_context, element, css,
	                                        html_stack);
	if (!selector) return;

	apply_css_selector_style(html_context, element, selector);

	done_css_selector(selector);
}
