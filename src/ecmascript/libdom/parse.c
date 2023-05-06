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

void *
document_parse_text(char *data, size_t length)
{
	dom_hubbub_parser *parser = NULL;
	dom_hubbub_error error;
	dom_hubbub_parser_params params;
	dom_document *doc;

	params.enc = NULL;
	params.fix_enc = true;
	params.enable_script = false;
	params.msg = NULL;
	params.script = NULL;
	params.ctx = NULL;
	params.daf = NULL;

	/* Create Hubbub parser */
	error = dom_hubbub_parser_create(&params, &parser, &doc);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Can't create Hubbub Parser\n");
		return NULL;
	}

	/* Parse */
	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t *)data, length);
	if (error != DOM_HUBBUB_OK) {
		dom_hubbub_parser_destroy(parser);
		fprintf(stderr, "Parsing errors occur\n");
		return NULL;
	}

	/* Done parsing file */
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		dom_hubbub_parser_destroy(parser);
		fprintf(stderr, "Parsing error when construct DOM\n");
		return NULL;
	}

	/* Finished with parser */
	dom_hubbub_parser_destroy(parser);

	return doc;
}

void *
document_parse(struct document *document)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct cache_entry *cached = document->cached;
	struct fragment *f = get_cache_fragment(cached);

	if (!f || !f->length) {
		return NULL;
	}

	return document_parse_text(f->data, f->length);
}

void
free_document(void *doc)
{
	if (!doc) {
		return;
	}
	dom_node *ddd = (dom_node *)doc;
	dom_node_unref(ddd);
}

void
el_insert_before(struct document *document, void *element, struct string *source)
{
	dom_document *doc = (dom_document *)document->dom;
	dom_node *node = (dom_node *)element;
	dom_string *text = NULL;
	dom_exception exc = dom_string_create(source->source, source->length, &text);

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
