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
#include "document/libdom/corestrings.h"
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

bool
convert_key_to_dom_string(term_event_key_T key, dom_string **res)
{
	bool is_special = key <= 0x001F || (0x007F <= key && key <= 0x009F);
	dom_string *dom_key = NULL;
	dom_exception exc;

	if (is_special) {
		switch (key) {
		case KBD_ENTER:
			dom_key = dom_string_ref(corestring_dom_Enter);
			break;
		case KBD_LEFT:
			dom_key = dom_string_ref(corestring_dom_ArrowLeft);
			break;
		case KBD_RIGHT:
			dom_key = dom_string_ref(corestring_dom_ArrowRight);
			break;
		case KBD_UP:
			dom_key = dom_string_ref(corestring_dom_ArrowUp);
			break;
		case KBD_DOWN:
			dom_key = dom_string_ref(corestring_dom_ArrowDown);
			break;
		case KBD_PAGE_UP:
			dom_key = dom_string_ref(corestring_dom_PageUp);
			break;
		case KBD_PAGE_DOWN:
			dom_key = dom_string_ref(corestring_dom_PageDown);
			break;
		case KBD_HOME:
			dom_key = dom_string_ref(corestring_dom_Home);
			break;
		case KBD_END:
			dom_key = dom_string_ref(corestring_dom_End);
			break;
		case KBD_ESC:
			dom_key = dom_string_ref(corestring_dom_Escape);
			break;
		case KBD_BS: // Backspace
			dom_key = dom_string_ref(corestring_dom_Backspace);
			break;
		case KBD_TAB: // Tab
			dom_key = dom_string_ref(corestring_dom_Tab);
			break;
		case KBD_INS: // Insert
			dom_key = dom_string_ref(corestring_dom_Insert);
			break;
		case KBD_DEL: // Delete
			dom_key = dom_string_ref(corestring_dom_Delete);
			break;
		case KBD_F1: // F1
			dom_key = dom_string_ref(corestring_dom_F1);
			break;
		case KBD_F2: // F2
			dom_key = dom_string_ref(corestring_dom_F2);
			break;
		case KBD_F3: // F3
			dom_key = dom_string_ref(corestring_dom_F3);
			break;
		case KBD_F4: // F4
			dom_key = dom_string_ref(corestring_dom_F4);
			break;
		case KBD_F5: // F5
			dom_key = dom_string_ref(corestring_dom_F5);
			break;
		case KBD_F6: // F6
			dom_key = dom_string_ref(corestring_dom_F6);
			break;
		case KBD_F7: // F7
			dom_key = dom_string_ref(corestring_dom_F7);
			break;
		case KBD_F8: // F8
			dom_key = dom_string_ref(corestring_dom_F8);
			break;
		case KBD_F9: // F9
			dom_key = dom_string_ref(corestring_dom_F9);
			break;
		case KBD_F10: // F10
			dom_key = dom_string_ref(corestring_dom_F10);
			break;
		case KBD_F11: // F11
			dom_key = dom_string_ref(corestring_dom_F11);
			break;
		case KBD_F12: // F12
			dom_key = dom_string_ref(corestring_dom_F12);
			break;
		default:
			dom_key = NULL;
			break;
		}
	} else {
		char *utf8 = encode_utf8(key);

		if (utf8) {
			exc = dom_string_create((const uint8_t *)utf8, strlen(utf8), &dom_key);

			if (exc != DOM_NO_ERR) {
				return false;
			}
		}
	}
	*res = dom_key;
	return true;
}
