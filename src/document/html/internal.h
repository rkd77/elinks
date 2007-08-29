
#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

/* Internal HTML engine (parsers + renderer) usage. Currently hides only some
 * hairy parser-specific accessors. */

/* This file must be included as the last one in the include list precisely
 * because of the hairy macros. */


#ifdef CONFIG_DOM_HTML

#include "document/html/domparser/domparser.h"

#define format		(domctx(html_context)->attr)
#define par_format	(domctx(html_context)->parattr)

#else

#include "document/html/mikuparser/mikuparser.h"

#define html_top	((struct html_element *) html_context->stack.next)
#define html_bottom	((struct html_element *) html_context->stack.prev)
#define format		(html_top->attr)
#define par_format	(html_top->parattr)

#define get_html_max_width() \
	int_max(par_format.width - (par_format.leftmargin + par_format.rightmargin), 0)

#endif

#define html_is_preformatted() (format.style.attr & AT_PREFORMATTED)


#endif
