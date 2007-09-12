#ifndef EL__DOCUMENT_DOM_SOURCE_H
#define EL__DOCUMENT_DOM_SOURCE_H

struct dom_renderer;
struct dom_stack;
struct string;

void init_dom_source_renderer(struct dom_stack *stack, struct dom_renderer *renderer,
			      struct string *buffer);

#endif
