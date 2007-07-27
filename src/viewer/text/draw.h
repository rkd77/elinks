
#ifndef EL__VIEWER_TEXT_DRAW_H
#define EL__VIEWER_TEXT_DRAW_H

struct document_view;
struct session;

/** Render and draw the current session document.
 * If @a rerender is:
 *	- 0 only redrawing is done
 *	- 1 render by first checking the document cache else rerender
 *	- 2 forces document to be rerendered */
void draw_formatted(struct session *ses, int rerender);

/** Update the document view, including frames and the status messages */
void refresh_view(struct session *ses, struct document_view *doc_view, int frames);

#endif
