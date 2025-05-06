/* Parse document. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "util/string.h"

void
el_insert_before(struct document *document, void *element, struct string *source)
{
	ELOG
	dom_html_document *doc = (dom_html_document *)document->dom;
	dom_node *node = (dom_node *)element;
	dom_string *text = NULL;
	dom_exception exc = dom_string_create((const unsigned char *)source->source, source->length, &text);

	if (exc != DOM_NO_ERR || !text) {
		return;
	}
	dom_text *frag = NULL;
	exc = dom_document_create_text_node(doc, text, &frag);

	if (exc != DOM_NO_ERR || !frag) {
		goto ret2;
	}
	dom_node *parent = NULL;
	exc = dom_node_get_parent_node(node, &parent);

	if (exc != DOM_NO_ERR || !parent) {
		goto ret3;
	}
	dom_node *result = NULL;
	exc = dom_node_replace_child(parent, frag, node, &result);

	if (exc != DOM_NO_ERR || !result) {
		goto ret4;
	}
	dom_node_unref(result);
ret4:
	dom_node_unref(parent);
ret3:
	dom_node_unref(frag);
ret2:
	dom_string_unref(text);
}
