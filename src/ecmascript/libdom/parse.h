#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_PARSE_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct string;

void *document_parse_text(char *data, size_t length);
void *document_parse(struct document *document);
void el_insert_before(struct document *document, void *element, struct string *source);

#ifdef __cplusplus
}
#endif

#endif
