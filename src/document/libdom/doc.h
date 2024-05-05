#ifndef EL__DOCUMENT_LIBDOM_DOC_H
#define EL__DOCUMENT_LIBDOM_DOC_H

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct string;

void *document_parse_text(const char *charset, char *data, size_t length);
void *document_parse(struct document *document, struct string *source);
void free_document(void *doc);
void *el_match_selector(const char *selector, void *node);

#ifdef __cplusplus
}
#endif

#endif
