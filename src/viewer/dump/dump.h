
#ifndef EL__VIEWER_DUMP_DUMP_H
#define EL__VIEWER_DUMP_DUMP_H

#include "util/lists.h"

struct string_;
struct document;

/* Adds the content of the document to the string line by line. */
struct string_ *
add_document_to_string(struct string_ *string, struct document *document);

int dump_to_file(struct document *, int);
void dump_next(LIST_OF(struct string_list_item) *);

#endif
