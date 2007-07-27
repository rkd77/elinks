#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "terminal/draw.h"
#include "util/lists.h"
#include "util/box.h"


struct document;
struct view_state;

struct document_view {
	LIST_HEAD(struct document_view);

	unsigned char *name;
	unsigned char **search_word;

	struct session *session;
	struct document *document;
	struct view_state *vs;

	struct box box;	/**< pos and size of window */
	int last_x, last_y; /**< last pos of window */
	int depth;
	int used;
};

#define get_old_current_link(doc_view) \
	(((doc_view) \
	  && (doc_view)->vs->old_current_link >= 0 \
	  && (doc_view)->vs->old_current_link < (doc_view)->document->nlinks) \
	? &(doc_view)->document->links[(doc_view)->vs->old_current_link] : NULL)

#define get_current_link(doc_view) \
	(((doc_view) \
	  && (doc_view)->vs->current_link >= 0 \
	  && (doc_view)->vs->current_link < (doc_view)->document->nlinks) \
	? &(doc_view)->document->links[(doc_view)->vs->current_link] : NULL)

#endif
