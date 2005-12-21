
#ifndef EL__DOCUMENT_DOM_RENDERER_H
#define EL__DOCUMENT_DOM_RENDERER_H

struct cache_entry;
struct document;
struct string;

void render_dom_document(struct cache_entry *cached, struct document *document, struct string *buffer);

/* Define to have debug info about the nodes added printed to the log.
 * Run as: ELINKS_LOG=/tmp/dom-dump.txt ./elinks -no-connect <url>
 * to have the debug dumped into a file. */
#define DOM_TREE_RENDERER

#ifdef DOM_TREE_RENDERER
extern struct dom_stack_context_info dom_tree_renderer_context_info;
#endif

#endif
