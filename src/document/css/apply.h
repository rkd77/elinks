/* $Id: apply.h,v 1.10.6.1 2005/01/29 01:01:21 jonas Exp $ */

#ifndef EL__DOCUMENT_CSS_APPLY_H
#define EL__DOCUMENT_CSS_APPLY_H

#include "util/lists.h"

struct css_stylesheet;
struct html_element;

/* This is the main entry point for the CSS micro-engine. It throws all the
 * power of the stylesheets at a given element. */

/* Gather all style information for the given @element, so it can later be
 * applied. Returned value should be freed using done_css_selector(). */
struct css_selector *
get_css_selector_for_element(struct html_element *element, struct css_stylesheet *css,
			     struct list_head *html_stack);


/* Apply properties from an existing selector. */
void apply_css_selector_style(struct html_element *element, struct css_selector *selector);

/* This function takes @element and applies its 'style' attribute onto its
 * attributes (if it contains such an attribute). */
void css_apply(struct html_element *element, struct css_stylesheet *css,
	       struct list_head *html_stack);

#endif
