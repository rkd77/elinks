#ifndef EL__DOCUMENT_XML_RENDERER2_H
#define EL__DOCUMENT_XML_RENDERER2_H

#ifdef __cplusplus
extern "C" {
#endif

struct cache_entry;
struct document;
struct string;

bool dump_dom_structure(struct source_renderer *renderer, void *nod, int depth);
void render_xhtml_document(struct cache_entry *cached, struct document *document, struct string *buffer);


#ifdef __cplusplus
}
#endif

#endif
