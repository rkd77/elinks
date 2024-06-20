#ifndef EL__DOCUMENT_LIBDOM_DOC_H
#define EL__DOCUMENT_LIBDOM_DOC_H

#include <dom/dom.h>
#include "terminal/kbd.h"

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct string;

void *document_parse_text(const char *charset, char *data, size_t length);
void *document_parse(struct document *document, struct string *source);
void free_document(void *doc);
void *el_match_selector(const char *selector, void *node);
void add_lowercase_to_string(struct string *buf, const char *str, int len);
bool convert_key_to_dom_string(term_event_key_T key, dom_string **res);
unicode_val_T convert_dom_string_to_keycode(dom_string *dom_key);
void keybstrings_init(void);
void keybstrings_fini(void);

void js_html_document_user_data_handler(dom_node_operation operation, dom_string *key, void *data, struct dom_node *src, struct dom_node *dst);

#ifdef __cplusplus
}
#endif

#endif
