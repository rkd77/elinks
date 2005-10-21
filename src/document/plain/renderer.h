
#ifndef EL__DOCUMENT_PLAIN_RENDERER_H
#define EL__DOCUMENT_PLAIN_RENDERER_H

struct cache_entry;
struct document;
struct string;

void render_plain_document(struct cache_entry *cached, struct document *document, struct string *buffer);

#endif
