
#ifndef EL__DOCUMENT_HTML_DOMPARSER_ELEMENTS_H
#define EL__DOCUMENT_HTML_DOMPARSER_ELEMENTS_H

#include "dom/sgml/html/html.h"


struct html_context;

struct dom_element_handlers {
	/* All the elements have struct html_element accessible as html_top. */

	/* Called when all element attributes are loaded, just before content
	 * gets processed. Elements requiring special formatting handling want
	 * to hook here. */
	void (*begins)(struct html_context *html_context);
	/* Called when element's content is over and the element is about to
	 * get popped from the stack. Non-paired elements or elements doing
	 * something special with their contents want to hook here. */
	void (*pop)(struct html_context *html_context);
};

extern struct dom_element_handlers dom_element_handlers[HTML_ELEMENTS];

#define dom_element_handle(type_, hook, ctx) \
	do { \
		int type = (type_); \
		if (dom_element_handlers[type].hook) \
			dom_element_handlers[type].hook((ctx)); \
	} while (0)

void dom_setup_element_handlers(void);

#endif
