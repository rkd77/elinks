#ifndef EL__DOCUMENT_VIEW_H
#define EL__DOCUMENT_VIEW_H

#include "terminal/draw.h"
#include "util/lists.h"
#include "util/box.h"


struct document;
struct view_state;

struct link_bg {
	int x, y;
	struct screen_char c;
};

struct document_view {
	LIST_HEAD(struct document_view);

	unsigned char *name;
	unsigned char **search_word;

	struct session *session;
	struct document *document;
	struct view_state *vs;

	/* The elements of this array correspond to the points of the active
	 * link. Each element stores the colour and attributes
	 * that the corresponding point had before the link was selected.
	 * The last element is an exception: it is the template character,
	 * which holds the colour and attributes that the active link was
	 * given upon being selected. */
	/* XXX: The above is guesswork; please confirm or correct. -- Miciah */
	struct link_bg *link_bg;

	int link_bg_n; /* number of elements in link_bg,
			  not including the template  */
	struct box box;	/* pos and size of window */
	int last_x, last_y; /* last pos of window */
	int depth;
	int used;
};

#define get_current_link(doc_view) \
	(((doc_view) \
	  && (doc_view)->vs->current_link >= 0 \
	  && (doc_view)->vs->current_link < (doc_view)->document->nlinks) \
	? &(doc_view)->document->links[(doc_view)->vs->current_link] : NULL)

#endif
