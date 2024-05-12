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

static const char *__keys_names[] = {
	"Enter",
	"ArrowLeft",
	"ArrowRight",
	"ArrowUp",
	"ArrowDown",
	"PageUp",
	"PageDown",
	"Home",
	"End",
	"Escape",
	"Backspace",
	"Tab",
	"Insert",
	"Delete",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
	NULL
};

static enum {
	KEYB_ENTER,
	KEYB_ARROW_LEFT,
	KEYB_ARROW_RIGHT,
	KEYB_ARROW_UP,
	KEYB_ARROW_DOWN,
	KEYB_PAGE_UP,
	KEYB_PAGE_DOWN,
	KEYB_HOME,
	KEYB_END,
	KEYB_ESCAPE,
	KEYB_BACKSPACE,
	KEYB_TAB,
	KEYB_INSERT,
	KEYB_DELETE,
	KEYB_F1,
	KEYB_F2,
	KEYB_F3,
	KEYB_F4,
	KEYB_F5,
	KEYB_F6,
	KEYB_F7,
	KEYB_F8,
	KEYB_F9,
	KEYB_F10,
	KEYB_F11,
	KEYB_F12,
	KEYB_COUNT
};

static lwc_string *keyb_lwc[KEYB_COUNT];

static void
initialize_keyb(void)
{
	int i;

	for (i = 0; i < KEYB_COUNT; i++) {
		dom_exception err = lwc_intern_string(__keys_names[i], strlen(__keys_names[i]), &keyb_lwc[i]);

		if (err != lwc_error_ok) {
			return;
			//return _dom_exception_from_lwc_error(err);
		}
	}
}

static void
finalize_keyb(void)
{
	int i;

	for (i = 0; i < KEYB_COUNT; i++) {
		if (keyb_lwc[i] != NULL) {
			lwc_string_unref(keyb_lwc[i]);
		}
	}
}

unicode_val_T
convert_dom_string_to_keycode(dom_string *dom_key)
{
	static int initialized = 0;

	if (!initialized) {
		initialize_keyb();
		initialized = 1;
	}
	if (!dom_key) {
		return 0;
	}

	int et = -1;
	lwc_string *t = NULL;
	dom_exception err;

	int i;
	err = dom_string_intern(dom_key, &t);

	if (err != DOM_NO_ERR) {
		return 0;
	}
	assert(t != NULL);

	for (i = 0; i < KEYB_COUNT; i++) {
		if (keyb_lwc[i] == t) {
			et = i;
			break;
		}
	}
	lwc_string_unref(t);

	switch (et) {
	case KEYB_ENTER:
		return 13;
	case KEYB_ARROW_LEFT:
		return 37;
	case KEYB_ARROW_RIGHT:
		return 39;
	case KEYB_ARROW_UP:
		return 38;
	case KEYB_ARROW_DOWN:
		return 40;
	case KEYB_PAGE_UP:
		return 33;
	case KEYB_PAGE_DOWN:
		return 34;
	case KEYB_HOME:
		return 36;
	case KEYB_END:
		return 35;
	case KEYB_ESCAPE:
		return 27;
	case KEYB_BACKSPACE:
		return 8;
	case KEYB_TAB:
		return 9;
	case KEYB_INSERT:
		return 45;
	case KEYB_DELETE:
		return 46;
	case KEYB_F1:
		return 112;
	case KEYB_F2:
		return 113;
	case KEYB_F3:
		return 114;
	case KEYB_F4:
		return 115;
	case KEYB_F5:
		return 116;
	case KEYB_F6:
		return 117;
	case KEYB_F7:
		return 118;
	case KEYB_F8:
		return 119;
	case KEYB_F9:
		return 120;
	case KEYB_F10:
		return 121;
	case KEYB_F11:
		return 122;
	case KEYB_F12:
		return 123;
	default:
	{
		char *utf8 = dom_string_data(dom_key);
		char *end = utf8 + dom_string_length(dom_key);
		return utf8_to_unicode(&utf8, end);
	}
	}
}
