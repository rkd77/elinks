#ifndef EL__DOCUMENT_LIBDOM_RENDERER2_H
#define EL__DOCUMENT_LIBDOM_RENDERER2_H

#ifdef __cplusplus
extern "C" {
#endif

struct cache_entry;
struct document;
struct document_view;
struct el_box;
struct string;
struct terminal;

struct node_rect {
	int x0, y0, x1, y1, offset;
};

void render_xhtml_document(struct cache_entry *cached, struct document *document, struct string *buffer);
void dump_xhtml(struct cache_entry *cached, struct document *document, int parse);

void free_libdom(void);
void debug_dump_xhtml(void *doc);
void debug_dump_xhtml2(void *node);

int fire_generic_dom_event(void *typ, void *target, int bubbles, int cancelable);
int fire_onload(void *doc);

#if 0
void walk2(struct document *document);
void scan_document(struct document_view *doc_view);
void try_to_color(struct terminal *term, struct el_box *box, struct document *document, int vx, int vy);
struct node_rect *get_element_rect(struct document *document, int offset);
#endif

#ifdef __cplusplus
}
#endif

#endif
