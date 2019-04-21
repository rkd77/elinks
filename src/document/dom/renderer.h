
#ifndef EL__DOCUMENT_DOM_RENDERER_H
#define EL__DOCUMENT_DOM_RENDERER_H

struct cache_entry;
struct document;
struct string;

void render_dom_document(struct cache_entry *cached, struct document *document, struct string *buffer);

#endif
