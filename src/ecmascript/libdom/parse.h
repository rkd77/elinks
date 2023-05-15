#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_PARSE_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct string;

void el_insert_before(struct document *document, void *element, struct string *source);

#ifdef __cplusplus
}
#endif

#endif
