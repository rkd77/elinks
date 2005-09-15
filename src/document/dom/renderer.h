/* $Id: renderer.h,v 1.3 2004/09/24 02:08:14 jonas Exp $ */

#ifndef EL__DOCUMENT_DOM_RENDERER_H
#define EL__DOCUMENT_DOM_RENDERER_H

struct cache_entry;
struct document;
struct string;

void render_dom_document(struct cache_entry *cached, struct document *document, struct string *buffer);

#endif
