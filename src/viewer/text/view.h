/* $Id: view.h,v 1.70.4.1 2005/01/29 01:33:34 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_VIEW_H
#define EL__VIEWER_TEXT_VIEW_H

struct document_view;
struct session;
struct term_event;
struct terminal;

enum frame_event_status {
	/* The event was not handled */
	FRAME_EVENT_IGNORED,
	/* The event was handled, and the screen should be redrawn */
	FRAME_EVENT_REFRESH,
	/* The event was handled, and the screen should _not_ be redrawn */
	FRAME_EVENT_OK,
	/* The event was handled, and the current session was destroyed */
	FRAME_EVENT_SESSION_DESTROYED,
};

/* Releases the document view's resources. But doesn't free() the @view. */
void detach_formatted(struct document_view *doc_view);

enum frame_event_status move_page_down(struct session *ses, struct document_view *doc_view);
enum frame_event_status move_page_up(struct session *ses, struct document_view *doc_view);
enum frame_event_status move_link(struct session *ses, struct document_view *doc_view,
				  int direction, int wraparound_bound, int wraparound_link);

#define move_link_next(ses, doc_view) move_link(ses, doc_view,  1, (doc_view)->document->nlinks - 1, 0)
#define move_link_prev(ses, doc_view) move_link(ses, doc_view, -1, 0, (doc_view)->document->nlinks - 1)

enum frame_event_status move_link_dir(struct session *ses, struct document_view *doc_view,
				      int dir_x, int dir_y);

#define move_link_up(ses, doc_view) move_link_dir(ses, doc_view,  0, -1)
#define move_link_down(ses, doc_view) move_link_dir(ses, doc_view,  0,  1)
#define move_link_left(ses, doc_view) move_link_dir(ses, doc_view, -1,  0)
#define move_link_right(ses, doc_view) move_link_dir(ses, doc_view,  1,  0)

enum frame_event_status scroll_up(struct session *ses, struct document_view *doc_view);
enum frame_event_status scroll_down(struct session *ses, struct document_view *doc_view);
enum frame_event_status scroll_left(struct session *ses, struct document_view *doc_view);
enum frame_event_status scroll_right(struct session *ses, struct document_view *doc_view);

enum frame_event_status move_document_start(struct session *ses, struct document_view *doc_view);
enum frame_event_status move_document_end(struct session *ses, struct document_view *doc_view);

enum frame_event_status set_frame(struct session *ses, struct document_view *doc_view, int xxxx);
struct document_view *current_frame(struct session *);

/* Used for changing between formatted and source (plain) view. */
void toggle_plain_html(struct session *ses, struct document_view *doc_view, int xxxx);

enum frame_event_status move_cursor_left(struct session *ses,
					 struct document_view *view);
enum frame_event_status move_cursor_right(struct session *ses,
					  struct document_view *view);
enum frame_event_status move_cursor_up(struct session *ses,
				       struct document_view *view);
enum frame_event_status move_cursor_down(struct session *ses,
					 struct document_view *view);

enum frame_event_status move_cursor(struct session *ses,
				    struct document_view *doc_view,
				    int x, int y);

/* Used for changing wrapping of text */
void toggle_wrap_text(struct session *ses, struct document_view *doc_view, int xxxx);

enum frame_event_status copy_current_link_to_clipboard(struct session *ses,
						struct document_view *doc_view,
						int xxx);

/* If the user has provided a numeric prefix, jump to the link
 * with that number as its index. */
int try_jump_to_link_number(struct session *ses,
			    struct document_view *doc_view);

/* File menu handlers. */

enum frame_event_status save_as(struct session *ses, struct document_view *doc_view, int magic);

/* Various event emitters and link menu handlers. */

void send_event(struct session *, struct term_event *);

enum frame_event_status save_formatted_dlg(struct session *ses, struct document_view *doc_view, int xxxx);
enum frame_event_status view_image(struct session *ses, struct document_view *doc_view, int xxxx);
enum frame_event_status download_link(struct session *ses, struct document_view *doc_view, int action);

#endif
