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
#include "document/libdom/doc.h"
#include "intl/charsets.h"
#include "util/string.h"

void *
document_parse_text(const char *charset, char *data, size_t length)
{
	dom_hubbub_parser *parser = NULL;
	dom_hubbub_error error;
	dom_hubbub_parser_params params;
	dom_document *doc;

	params.enc = charset;
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
document_parse(struct document *document, struct string *source)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	const char *charset = document->cp >= 0 ? get_cp_mime_name(document->cp) : "";

	return document_parse_text(charset, source->source, source->length);
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
add_lowercase_to_string(struct string *buf, const char *str, int len)
{
	char *tmp = memacpy(str, len);
	int i;

	if (!tmp) {
		return;
	}

	for (i = 0; i < len; i++) {
		if (tmp[i] >= 'A' && tmp[i] <= 'Z') {
			tmp[i] += 32;
		}
	}
	add_bytes_to_string(buf, tmp, len);
	mem_free(tmp);
}
