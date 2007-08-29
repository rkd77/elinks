
#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

/* Internal HTML engine (parsers + renderer) usage. Currently hides only some
 * hairy parser-specific accessors. */

/* This file must be included as the last one in the include list precisely
 * because of the hairy macros. */

#define html_top	((struct html_element *) html_context->stack.next)
#define html_bottom	((struct html_element *) html_context->stack.prev)
#define format		(html_top->attr)
#define par_format	(html_top->parattr)

#define get_html_max_width() \
	int_max(par_format.width - (par_format.leftmargin + par_format.rightmargin), 0)

#define html_is_preformatted() (format.style.attr & AT_PREFORMATTED)


#endif
