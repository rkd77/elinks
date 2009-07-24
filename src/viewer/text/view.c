/** HTML viewer (and much more)
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/leds.h"
#include "bfu/menu.h"
#include "bfu/dialog.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/document.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/options.h"
#include "document/renderer.h"
#include "document/view.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"
#include "viewer/action.h"
#include "viewer/dump/dump.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/marks.h"
#include "viewer/text/search.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

void
detach_formatted(struct document_view *doc_view)
{
	assert(doc_view);
	if_assert_failed return;

#ifdef CONFIG_ECMASCRIPT
	if (doc_view->session) {
		mem_free_set(&doc_view->session->status.window_status, NULL);
	}
#endif
	if (doc_view->document) {
		release_document(doc_view->document);
		doc_view->document = NULL;
	}
	if (doc_view->vs) {
		doc_view->vs->doc_view = NULL;
		doc_view->vs = NULL;
	}
	mem_free_set(&doc_view->name, NULL);
}

/*! @a type == 0 -> PAGE_DOWN;
 * @a type == 1 -> DOWN */
static void
move_down(struct session *ses, struct document_view *doc_view, int type)
{
	int newpos;

	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	assert(ses->navigate_mode == NAVIGATE_LINKWISE);	/* XXX: drop it at some time. --Zas */

	newpos = doc_view->vs->y + doc_view->box.height;
	if (newpos < doc_view->document->height)
		doc_view->vs->y = newpos;

	if (current_link_is_visible(doc_view))
		return;

	if (type)
		find_link_down(doc_view);
	else
		find_link_page_down(doc_view);

	return;
}

enum frame_event_status
move_page_down(struct session *ses, struct document_view *doc_view)
{
	int oldy = doc_view->vs->y;
	int count = eat_kbd_repeat_count(ses);

	ses->navigate_mode = NAVIGATE_LINKWISE;

	do move_down(ses, doc_view, 0); while (--count > 0);

	return doc_view->vs->y == oldy ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

/*! @a type == 0 -> PAGE_UP;
 * @a type == 1 -> UP */
static void
move_up(struct session *ses, struct document_view *doc_view, int type)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	assert(ses->navigate_mode == NAVIGATE_LINKWISE);	/* XXX: drop it at some time. --Zas */

	if (doc_view->vs->y == 0) return;

	doc_view->vs->y -= doc_view->box.height;
	int_lower_bound(&doc_view->vs->y, 0);

	if (current_link_is_visible(doc_view))
		return;

	if (type)
		find_link_up(doc_view);
	else
		find_link_page_up(doc_view);

	return;
}

enum frame_event_status
move_page_up(struct session *ses, struct document_view *doc_view)
{
	int oldy = doc_view->vs->y;
	int count = eat_kbd_repeat_count(ses);

	ses->navigate_mode = NAVIGATE_LINKWISE;

	do move_up(ses, doc_view, 0); while (--count > 0);

	return doc_view->vs->y == oldy ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}


enum frame_event_status
move_link(struct session *ses, struct document_view *doc_view, int direction,
	  int wraparound_bound, int wraparound_link)
{
	int wraparound = 0;
	int count;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	ses->navigate_mode = NAVIGATE_LINKWISE;

	if (doc_view->document->nlinks < 2) {
		/* Wraparound only makes sense with more than one link. */
		wraparound_bound = -1;
	} else {
		/* We only bother this option if there's some links
		 * in document. */
		wraparound = get_opt_bool("document.browse.links.wraparound");
	}

	count = eat_kbd_repeat_count(ses);

	do {
		int current_link = doc_view->vs->current_link;

		if (current_link == wraparound_bound) {
			if (wraparound) {
				jump_to_link_number(ses, doc_view, wraparound_link);
				/* FIXME: This needs further work, we should call
				 * page_down() and set_textarea() under some conditions
				 * as well. --pasky */
				continue;
			}

		} else {
			if (next_link_in_view_y(doc_view, current_link + direction,
					        direction))
				continue;
		}

		/* This is a work around for the case where the index of
		 * @wraparound_bound is not necessarily the index of the first
		 * or the last link in the view. It means that the link moving
		 * could end up calling next_link_in_view() in the condition
		 * above. This is bad because next_link_in_view() will then
		 * 'reset' doc_view->vs->current_link to -1 and the effect will
		 * be that the current link will 'wrap around'. By restoring
		 * the index of the @current_link nothing will be wrapped
		 * around and move_{up,down} will take care of finding the next
		 * link. */
		doc_view->vs->current_link = current_link;

		if (direction > 0) {
			move_down(ses, doc_view, 1);
		} else {
			move_up(ses, doc_view, 1);
		}

		if (current_link != wraparound_bound
		    && current_link != doc_view->vs->current_link) {
			set_textarea(doc_view, -direction);
		}
	} while (--count > 0);

	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
move_link_dir(struct session *ses, struct document_view *doc_view, int dir_x, int dir_y)
{
	int count;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	ses->navigate_mode = NAVIGATE_LINKWISE;

	count = eat_kbd_repeat_count(ses);

	do {
		int current_link = doc_view->vs->current_link;

		if (next_link_in_dir(doc_view, dir_x, dir_y))
			continue;

		/* FIXME: This won't preserve the column! */
		if (dir_y > 0)
			move_down(ses, doc_view, 1);
		else if (dir_y < 0)
			move_up(ses, doc_view, 1);

		if (dir_y && current_link != doc_view->vs->current_link) {
			set_textarea(doc_view, -dir_y);
		}
	} while (--count > 0);

	return FRAME_EVENT_REFRESH;
}

/*! @a steps > 0 -> down */
static enum frame_event_status
vertical_scroll(struct session *ses, struct document_view *doc_view, int steps)
{
	int y;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	y = doc_view->vs->y + steps;
	if (steps > 0) {
		/* DOWN */
		int max_height = doc_view->document->height - doc_view->box.height;

		if (doc_view->vs->y >= max_height) return FRAME_EVENT_OK;
		int_upper_bound(&y, max_height);
	}

	int_lower_bound(&y, 0);

	if (doc_view->vs->y == y) return FRAME_EVENT_OK;

	doc_view->vs->y = y;

	if (current_link_is_visible(doc_view))
		return FRAME_EVENT_REFRESH;

	if (steps > 0)
		find_link_page_down(doc_view);
	else
		find_link_page_up(doc_view);

	return FRAME_EVENT_REFRESH;
}

/*! @a steps > 0 -> right */
static enum frame_event_status
horizontal_scroll(struct session *ses, struct document_view *doc_view, int steps)
{
	int x, max;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	x = doc_view->vs->x + steps;

	if (get_opt_bool("document.browse.scrolling.horizontal_extended")) {
		max = doc_view->document->width - 1;
	} else {
		max = int_max(doc_view->vs->x,
			      doc_view->document->width - doc_view->box.width);
	}

	int_bounds(&x, 0, max);
	if (doc_view->vs->x == x) return FRAME_EVENT_OK;

	doc_view->vs->x = x;

	if (current_link_is_visible(doc_view))
		return FRAME_EVENT_REFRESH;

	find_link_page_down(doc_view);

	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
scroll_up(struct session *ses, struct document_view *doc_view)
{
	int steps = eat_kbd_repeat_count(ses);

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.vertical_step");

	return vertical_scroll(ses, doc_view, -steps);
}

enum frame_event_status
scroll_down(struct session *ses, struct document_view *doc_view)
{
	int steps = eat_kbd_repeat_count(ses);

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.vertical_step");

	return vertical_scroll(ses, doc_view, steps);
}

enum frame_event_status
scroll_left(struct session *ses, struct document_view *doc_view)
{
	int steps = eat_kbd_repeat_count(ses);

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.horizontal_step");

	return horizontal_scroll(ses, doc_view, -steps);
}

enum frame_event_status
scroll_right(struct session *ses, struct document_view *doc_view)
{
	int steps = eat_kbd_repeat_count(ses);

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.horizontal_step");

	return horizontal_scroll(ses, doc_view, steps);
}

#ifdef CONFIG_MOUSE
static enum frame_event_status
scroll_mouse_up(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.vertical_step");

	return vertical_scroll(ses, doc_view, -steps);
}

static enum frame_event_status
scroll_mouse_down(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.vertical_step");

	return vertical_scroll(ses, doc_view, steps);
}

static enum frame_event_status
scroll_mouse_left(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.horizontal_step");

	return horizontal_scroll(ses, doc_view, -steps);
}

static enum frame_event_status
scroll_mouse_right(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.horizontal_step");

	return horizontal_scroll(ses, doc_view, steps);
}
#endif /* CONFIG_MOUSE */

enum frame_event_status
move_document_start(struct session *ses, struct document_view *doc_view)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return FRAME_EVENT_OK;

	doc_view->vs->y = doc_view->vs->x = 0;

	if (ses->navigate_mode == NAVIGATE_CURSOR_ROUTING) {
		/* Move to the first line and the first column. */
		move_cursor(ses, doc_view, doc_view->box.x, doc_view->box.y);
	} else {
		find_link_page_down(doc_view);
	}

	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
move_document_end(struct session *ses, struct document_view *doc_view)
{
	int max_height;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	max_height = doc_view->document->height - doc_view->box.height;
	doc_view->vs->x = 0;
	int_lower_bound(&doc_view->vs->y, int_max(0, max_height));

	if (ses->navigate_mode == NAVIGATE_CURSOR_ROUTING) {
		/* Move to the last line of the document,
		 * but preserve the column. This is done to avoid
		 * moving the cursor backwards if it is already
		 * on the last line but is not on the first column. */
		move_cursor(ses, doc_view, ses->tab->x,
			    doc_view->document->height - doc_view->vs->y);
	} else {
		find_link_page_up(doc_view);
	}

	return FRAME_EVENT_REFRESH;
}

enum frame_event_status
set_frame(struct session *ses, struct document_view *doc_view, int xxxx)
{
	assert(ses && ses->doc_view && doc_view && doc_view->vs);
	if_assert_failed return FRAME_EVENT_OK;

	if (doc_view == ses->doc_view) return FRAME_EVENT_OK;
	goto_uri(ses, doc_view->vs->uri);
	ses->navigate_mode = NAVIGATE_LINKWISE;

	return FRAME_EVENT_OK;
}


void
toggle_plain_html(struct session *ses, struct document_view *doc_view, int xxxx)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	doc_view->vs->plain = !doc_view->vs->plain;
	draw_formatted(ses, 1);
}

void
toggle_wrap_text(struct session *ses, struct document_view *doc_view, int xxxx)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	doc_view->vs->wrap = !doc_view->vs->wrap;
	draw_formatted(ses, 1);
}

/** Move the cursor to the document view co-ordinates provided
 * as @a x and @a y, scroll the document if necessary, put us in
 * cursor-routing navigation mode if that is not the current mode,
 * and select any link under the cursor. */
enum frame_event_status
move_cursor(struct session *ses, struct document_view *doc_view, int x, int y)
{
	enum frame_event_status status = FRAME_EVENT_REFRESH;
	struct terminal *term = ses->tab->term;
	struct box *box = &doc_view->box;
	struct link *link;

	/* If cursor was moved outside the document view scroll it, but only
	 * within the document canvas */
	if (!is_in_box(box, x, y)) {
		int max_height = doc_view->document->height - doc_view->vs->y;
		int max_width = doc_view->document->width - doc_view->vs->x;

		if (y < box->y) {
			status = vertical_scroll(ses, doc_view, y - box->y);

		} else if (y >= box->y + box->height && y <= max_height) {
			status = vertical_scroll(ses, doc_view,
						 y - (box->y + box->height - 1));

		} else if (x < box->x) {
			status = horizontal_scroll(ses, doc_view, x - box->x);

		} else if (x >= box->x + box->width && x <= max_width) {
			status = horizontal_scroll(ses, doc_view,
						   x - (box->x + box->width - 1));
		}

		/* If the view was not scrolled there's nothing more to do */
		if (status != FRAME_EVENT_REFRESH)
			return status;

		/* Restrict the cursor position within the current view */
		int_bounds(&x, box->x, box->x + box->width - 1);
		int_bounds(&y, box->y, box->y + box->height - 1);
	}

	/* Scrolling could have changed the navigation mode */
	ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;

	link = get_link_at_coordinates(doc_view, x - box->x, y - box->y);
	if (link) {
		doc_view->vs->current_link = link - doc_view->document->links;
	} else {
		doc_view->vs->current_link = -1;
	}

	/* Set the unblockable cursor position and update the window pointer so
	 * stuff like the link menu will be drawn relative to the cursor. */
	set_cursor(term, x, y, 0);
	set_window_ptr(ses->tab, x, y);

	return status;
}

static enum frame_event_status
move_cursor_rel_count(struct session *ses, struct document_view *view,
		      int rx, int ry, int count)
{
	int x, y;

	x = ses->tab->x + rx*count;
	y = ses->tab->y + ry*count;
	return move_cursor(ses, view, x, y);
}

static enum frame_event_status
move_cursor_rel(struct session *ses, struct document_view *view,
	        int rx, int ry)
{
	int count = eat_kbd_repeat_count(ses);

	int_lower_bound(&count, 1);

	return move_cursor_rel_count(ses, view, rx, ry, count);
}

enum frame_event_status
move_cursor_left(struct session *ses, struct document_view *view)
{
	return move_cursor_rel(ses, view, -1, 0);
}

enum frame_event_status
move_cursor_right(struct session *ses, struct document_view *view)
{
	return move_cursor_rel(ses, view, 1, 0);
}

enum frame_event_status
move_cursor_up(struct session *ses, struct document_view *view)
{
	return move_cursor_rel(ses, view, 0, -1);
}

enum frame_event_status
move_cursor_down(struct session *ses, struct document_view *view)
{
	return move_cursor_rel(ses, view, 0, 1);
}

enum frame_event_status
move_link_up_line(struct session *ses, struct document_view *doc_view)
{
	struct document *document;
	struct view_state *vs;
	struct box *box;
	int min_y, y, y1;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;
	vs = doc_view->vs;
	document = doc_view->document;
	box = &doc_view->box;
	if (!document->lines1) {
		if (vs->y) {
			vs->y -= box->height;
			int_lower_bound(&vs->y, 0);
			return FRAME_EVENT_REFRESH;
		}
		return FRAME_EVENT_OK;
	}
	min_y = vs->y - box->height;
	int_lower_bound(&min_y, 0);
	y1 = y = vs->y + ses->tab->y - box->y;
	int_upper_bound(&y, document->height - 1);
	for (y--; y >= min_y; y--) {
		struct link *link = document->lines1[y];

		if (!link) continue;
		for (; link <= document->lines2[y]; link++) {
			enum frame_event_status status;

			if (link->points[0].y != y) continue;
			if (y < vs->y) {
				/* The line is above the visible part
				 * of the document.  Scroll it by one
				 * page, but not at all past the
				 * beginning of the document.  */
				int mini = int_min(box->height, vs->y);

				/* Before this update, y is the line
				 * number in the document, and y - y1
				 * is the number of lines the cursor
				 * must move in the document.
				 * Afterwards, y does not make sense,
				 * but y - y1 is the number of lines
				 * the cursor must move on the screen.  */
				vs->y -= mini;
				y += mini;
			}
			status = move_cursor_rel_count(ses, doc_view, 0, y - y1, 1);
			if (link == get_current_link(doc_view))
				ses->navigate_mode = NAVIGATE_LINKWISE;
			return status;
		}
	}
	if (vs->y) {
		vs->y -= box->height;
		int_lower_bound(&vs->y, 0);
		ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;
		return FRAME_EVENT_REFRESH;
	}
	return FRAME_EVENT_OK;
}

enum frame_event_status
move_link_down_line(struct session *ses, struct document_view *doc_view)
{
	struct document *document;
	struct view_state *vs;
	struct box *box;
	int max_y, y, y1;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;
	vs = doc_view->vs;
	document = doc_view->document;
	box = &doc_view->box;
	if (!document->lines1) {
		if (vs->y + box->height < document->height) {
			vs->y += box->height;
			return FRAME_EVENT_REFRESH;
		}
		return FRAME_EVENT_OK;
	}
	max_y = vs->y + box->height * 2 - 1;
	int_upper_bound(&max_y, document->height - 1);
	y1 = y = vs->y + ses->tab->y - box->y;
	for (y++; y <= max_y; y++) {
		struct link *link = document->lines1[y];

		if (!link) continue;
		for (; link <= document->lines2[y]; link++) {
			enum frame_event_status status;

			if (link->points[0].y != y) continue;
			if (y >= vs->y + box->height) {
				/* The line is below the visible part
				 * of the document.  Scroll it by one
				 * page, but keep at least one line of
				 * the document on the screen.  */
				int mini = int_min(box->height, document->height - vs->y - 1);

				/* Before this update, y is the line
				 * number in the document, and y - y1
				 * is the number of lines the cursor
				 * must move in the document.
				 * Afterwards, y does not make sense,
				 * but y - y1 is the number of lines
				 * the cursor must move on the screen.  */
				vs->y += mini;
				y -= mini;
			}
			status = move_cursor_rel_count(ses, doc_view, 0, y - y1, 1);
			if (link == get_current_link(doc_view))
				ses->navigate_mode = NAVIGATE_LINKWISE;
			return status;
		}
	}
	if (vs->y + box->height < document->height) {
		vs->y += box->height;
		ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;
		return FRAME_EVENT_REFRESH;
	}
	return FRAME_EVENT_OK;
}

enum frame_event_status
move_link_prev_line(struct session *ses, struct document_view *doc_view)
{
	struct view_state *vs;
	struct document *document;
	struct box *box;
	struct link *link, *last = NULL;
	int y1, y, min_y, min_x, max_x, x1;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	vs = doc_view->vs;
	document = doc_view->document;
	box = &doc_view->box;
	if (!document->lines1) {
		if (vs->y) {
			vs->y -= box->height;
			int_lower_bound(&vs->y, 0);
			return FRAME_EVENT_REFRESH;
		}
		return FRAME_EVENT_OK;
	}
	y = y1 = vs->y + ses->tab->y - box->y;
	x1 = vs->x + ses->tab->x - box->x;

	link = get_current_link(doc_view);
	if (link) {
		get_link_x_bounds(link, y1, &min_x, &max_x);		
	} else {
		min_x = max_x = x1;
	}
	int_upper_bound(&y, document->height - 1);
	min_y = int_max(0, vs->y - box->height);

	for (; y >= min_y; y--, min_x = INT_MAX) {
		link = document->lines1[y];
		if (!link) continue;
		for (; link <= document->lines2[y]; link++) {
			if (link->points[0].y != y) continue;
			if (link->points[0].x >= min_x) continue;
			if (!last) last = link;
			else if (link->points[0].x > last->points[0].x) last = link;
		}
		if (last) {
			enum frame_event_status status;

			y = last->points[0].y;
			if (y < vs->y) {
				/* The line is above the visible part
				 * of the document.  Scroll it by one
				 * page, but not at all past the
				 * beginning of the document.  */
				int mini = int_min(box->height, vs->y);

				/* Before this update, y is the line
				 * number in the document, and y - y1
				 * is the number of lines the cursor
				 * must move in the document.
				 * Afterwards, y does not make sense,
				 * but y - y1 is the number of lines
				 * the cursor must move on the screen.  */
				vs->y -= mini;
				y += mini;
			}
			status = move_cursor_rel_count(ses, doc_view, last->points[0].x - x1, y - y1, 1);
			if (last == get_current_link(doc_view))
				ses->navigate_mode = NAVIGATE_LINKWISE;
			return status;
		}
	}
	if (vs->y) {
		vs->y -= box->height;
		int_lower_bound(&vs->y, 0);
		ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;
		return FRAME_EVENT_REFRESH;
	}
	return FRAME_EVENT_OK;
}

enum frame_event_status
move_link_next_line(struct session *ses, struct document_view *doc_view)
{
	struct view_state *vs;
	struct document *document;
	struct box *box;
	struct link *link, *last = NULL;
	int y1, y, max_y, min_x, max_x, x1;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return FRAME_EVENT_OK;

	vs = doc_view->vs;
	document = doc_view->document;
	box = &doc_view->box;
	if (!document->lines1) {
		if (vs->y + box->height < document->height) {
			vs->y += box->height;
			return FRAME_EVENT_REFRESH;
		}
		return FRAME_EVENT_OK;
	}
	y = y1 = vs->y + ses->tab->y - box->y;
	x1 = vs->x + ses->tab->x - box->x;

	link = get_current_link(doc_view);
	if (link) {
		get_link_x_bounds(link, y1, &min_x, &max_x);		
	} else {
		min_x = max_x = x1;
	}
	int_upper_bound(&y, document->height - 1);
	max_y = int_min(vs->y + 2 * box->height - 1, document->height - 1);

	for (; y <= max_y; y++, min_x = -1) {
		link = document->lines1[y];
		if (!link) continue;
		for (; link <= document->lines2[y]; link++) {
			if (link->points[0].y != y) continue;
			if (link->points[0].x <= min_x) continue;
			if (!last) last = link;
			else if (link->points[0].x < last->points[0].x) last = link;
		}
		if (last) {
			enum frame_event_status status;

			y = last->points[0].y;
			if (y >= vs->y + box->height) {
				/* The line is below the visible part
				 * of the document.  Scroll it by one
				 * page, but keep at least one line of
				 * the document on the screen.  */
				int mini = int_min(box->height, document->height - vs->y - 1);

				/* Before this update, y is the line
				 * number in the document, and y - y1
				 * is the number of lines the cursor
				 * must move in the document.
				 * Afterwards, y does not make sense,
				 * but y - y1 is the number of lines
				 * the cursor must move on the screen.  */
				vs->y += mini;
				y -= mini;
			}
			status = move_cursor_rel_count(ses, doc_view, last->points[0].x - x1, y - y1, 1);
			if (last == get_current_link(doc_view))
				ses->navigate_mode = NAVIGATE_LINKWISE;
			return status;
		}
	}
	if (vs->y + box->height < document->height) {
		vs->y += box->height;
		ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;
		return FRAME_EVENT_REFRESH;
	}
	return FRAME_EVENT_OK;
}

enum frame_event_status
move_cursor_line_start(struct session *ses, struct document_view *doc_view)
{
	struct view_state *vs;
	struct box *box;
	int x;

	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return FRAME_EVENT_OK;

	vs = doc_view->vs;
	box = &doc_view->box;
	x = vs->x + ses->tab->x - box->x;
	return move_cursor_rel(ses, doc_view, -x, 0);
}

enum frame_event_status
copy_current_link_to_clipboard(struct session *ses,
			       struct document_view *doc_view,
			       int xxx)
{
	struct link *link;
	struct uri *uri;
	unsigned char *uristring;

	link = get_current_link(doc_view);
	if (!link) return FRAME_EVENT_OK;

	uri = get_link_uri(ses, doc_view, link);
	if (!uri) return FRAME_EVENT_OK;

	uristring = get_uri_string(uri, URI_ORIGINAL);
	done_uri(uri);

	if (uristring) {
		set_clipboard_text(uristring);
		mem_free(uristring);
	}

	return FRAME_EVENT_OK;
}


int
try_jump_to_link_number(struct session *ses, struct document_view *doc_view)
{
	int link_number = eat_kbd_repeat_count(ses) - 1;

	if (link_number < 0) return 1;

	if (!doc_view) return 0;

	if (link_number >= doc_view->document->nlinks)
		return 0;

	jump_to_link_number(ses, doc_view, link_number);
	refresh_view(ses, doc_view, 0);

	return 1;
}

#ifdef CONFIG_MARKS
enum frame_event_status
try_mark_key(struct session *ses, struct document_view *doc_view,
	     struct term_event *ev)
{
	term_event_key_T key = get_kbd_key(ev);
	unsigned char mark;

	/* set_mark and goto_mark allow only a subset of the ASCII
	 * character repertoire as mark characters.  If get_kbd_key(ev)
	 * is something else (i.e. a special key or a non-ASCII
	 * character), map it to an ASCII character that the functions
	 * will not accept, so the results are consistent.
	 * When CONFIG_UTF8 is not defined, this assumes that codes
	 * 0 to 0x7F in all codepages match ASCII.  */
	if (key >= 0 && key <= 0x7F)
		mark = (unsigned char) key;
	else
		mark = 0;

	switch (ses->kbdprefix.mark) {
		case KP_MARK_NOTHING:
			return FRAME_EVENT_IGNORED;

		case KP_MARK_SET:
			/* It is intentional to set the mark
			 * to NULL if !doc_view->vs. */
			set_mark(mark, doc_view->vs);
			break;

		case KP_MARK_GOTO:
			goto_mark(mark, doc_view->vs);
			break;
	}

	ses->kbdprefix.repeat_count = 0;
	ses->kbdprefix.mark = KP_MARK_NOTHING;

	return FRAME_EVENT_REFRESH;
}
#endif

static enum frame_event_status
try_prefix_key(struct session *ses, struct document_view *doc_view,
	       struct term_event *ev)
{
	struct document *document = doc_view->document;
	struct document_options *doc_opts = &document->options;
	int digit = get_kbd_key(ev) - '0';

	if (digit < 0 || digit > 9)
		return FRAME_EVENT_IGNORED;

	if (get_kbd_modifier(ev)
	    || ses->kbdprefix.repeat_count /* The user has already begun
	                                    * entering a prefix. */
	    || !doc_opts->num_links_key
	    || (doc_opts->num_links_key == 1 && !doc_opts->links_numbering)) {
		/* Repeat count.
		 * ses->kbdprefix.repeat_count is initialized to zero
		 * the first time by init_session() calloc() call.
		 * When used, it has to be reset to zero. */

		/* Clear the highlighting for the previous partial prefix. */
		if (ses->kbdprefix.repeat_count) draw_formatted(ses, 0);

		ses->kbdprefix.repeat_count *= 10;
		ses->kbdprefix.repeat_count += digit;

		/* If too big, just restart from zero, so pressing
		 * '0' six times or more will reset the count. */
		if (ses->kbdprefix.repeat_count > 99999)
			ses->kbdprefix.repeat_count = 0;
		else if (ses->kbdprefix.repeat_count)
			highlight_links_with_prefixes_that_start_with_n(
			                           ses->tab->term, doc_view,
			                           ses->kbdprefix.repeat_count);

		return FRAME_EVENT_OK;
	}

	if (digit >= 1 && !get_kbd_modifier(ev)) {
		int nlinks = document->nlinks, length;
		unsigned char d[2] = { get_kbd_key(ev), 0 };

		ses->kbdprefix.repeat_count = 0;

		if (!nlinks) return FRAME_EVENT_OK;

		for (length = 1; nlinks; nlinks /= 10)
			length++;

		input_dialog(ses->tab->term, NULL,
			     N_("Go to link"), N_("Enter link number"),
			     ses, NULL,
			     length, d, 1, document->nlinks, check_number,
			     (void (*)(void *, unsigned char *)) goto_link_number, NULL);

		return FRAME_EVENT_OK;
	}

	return FRAME_EVENT_IGNORED;
}

static enum frame_event_status
try_form_insert_mode(struct session *ses, struct document_view *doc_view,
		     struct link *link, struct term_event *ev)
{
	enum frame_event_status status = FRAME_EVENT_IGNORED;
	enum edit_action action_id;

	if (!link_is_textinput(link))
		return FRAME_EVENT_IGNORED;

	action_id = kbd_action(KEYMAP_EDIT, ev, NULL);

	if (ses->insert_mode == INSERT_MODE_OFF) {
		if (action_id == ACT_EDIT_ENTER) {
			ses->insert_mode = INSERT_MODE_ON;
			status = FRAME_EVENT_REFRESH;
		}
	}

	return status;
}

static enum frame_event_status
try_form_action(struct session *ses, struct document_view *doc_view,
		struct link *link, struct term_event *ev)
{
	enum frame_event_status status;

	assert(link);

	if (!link_is_textinput(link))
		return FRAME_EVENT_IGNORED;

	status = field_op(ses, doc_view, link, ev);

	if (status != FRAME_EVENT_IGNORED
	    && ses->insert_mode == INSERT_MODE_ON) {
		assert(link == get_current_link(doc_view));
	}

	return status;
}

static enum frame_event_status
frame_ev_kbd(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	enum frame_event_status status = FRAME_EVENT_IGNORED;
	int accesskey_priority;
	struct link *link = get_current_link(doc_view);

	if (link) {
		status = try_form_insert_mode(ses, doc_view, link, ev);
		if (status != FRAME_EVENT_IGNORED)
			return status;

		status = try_form_action(ses, doc_view, link, ev);
		if (status != FRAME_EVENT_IGNORED)
			return status;
	}

#ifdef CONFIG_MARKS
	status = try_mark_key(ses, doc_view, ev);
	if (status != FRAME_EVENT_IGNORED)
		return status;
#endif
	accesskey_priority = get_opt_int("document.browse.accesskey.priority");

	if (accesskey_priority >= 2) {
		status = try_document_key(ses, doc_view, ev);

		if (status != FRAME_EVENT_IGNORED) {
			/* The document ate the key! */
			return status;
		}
	}

	status = try_prefix_key(ses, doc_view, ev);
	if (status != FRAME_EVENT_IGNORED)
		return status;

	if (accesskey_priority == 1) {
		status = try_document_key(ses, doc_view, ev);

		if (status != FRAME_EVENT_IGNORED) {
			/* The document ate the key! */
			return status;
		}
	}

	return FRAME_EVENT_IGNORED;
}

#ifdef CONFIG_MOUSE
static enum frame_event_status
frame_ev_mouse(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	int x = ev->info.mouse.x;
	int y = ev->info.mouse.y;
	struct link *link;

	if (check_mouse_wheel(ev)) {
		if (!check_mouse_action(ev, B_DOWN)) {
			/* We handle only B_DOWN case... */
		} else if (check_mouse_button(ev, B_WHEEL_UP)) {
			return scroll_mouse_up(ses, doc_view);
		} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
			return scroll_mouse_down(ses, doc_view);
		}

		return FRAME_EVENT_OK;
	}

	link = get_link_at_coordinates(doc_view, x, y);
	if (link) {
		enum frame_event_status status = FRAME_EVENT_REFRESH;

		doc_view->vs->current_link = link - doc_view->document->links;
		ses->navigate_mode = NAVIGATE_LINKWISE;

		if (!link_is_textinput(link)) {

			status = FRAME_EVENT_OK;

			refresh_view(ses, doc_view, 0);

			if (check_mouse_button(ev, B_LEFT)
			     || check_mouse_button(ev, B_MIDDLE)) {
				if (check_mouse_action(ev, B_DOWN))
					do_not_ignore_next_mouse_event(ses->tab->term);
				else if (check_mouse_button(ev, B_LEFT))
					status = enter(ses, doc_view, 0);
				else if (check_mouse_button(ev, B_MIDDLE))
					open_current_link_in_new_tab(ses, 1);
			} else {
				link_menu(ses->tab->term, NULL, ses);
			}
		}

		return status;
	}

	if (check_mouse_button(ev, B_LEFT)) {
		/* Clicking the edge of screen will scroll the document. */

		int scrollmargin = get_opt_int("document.browse.scrolling.margin");

		/* XXX: This is code duplication with kbd handlers. But
		 * repeatcount-free here. */

		if (y < scrollmargin) {
			return scroll_mouse_up(ses, doc_view);
		}
		if (y >= doc_view->box.height - scrollmargin) {
			return scroll_mouse_down(ses, doc_view);
		}

		if (x < scrollmargin * 2) {
			return scroll_mouse_left(ses, doc_view);
		}
		if (x >= doc_view->box.width - scrollmargin * 2) {
			return scroll_mouse_right(ses, doc_view);
		}

		return FRAME_EVENT_OK;
	}

	return FRAME_EVENT_IGNORED;
}
#endif /* CONFIG_MOUSE */

static enum frame_event_status
frame_ev(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	assertm(doc_view && doc_view->document, "document not formatted");
	if_assert_failed return FRAME_EVENT_IGNORED;

	assert(ses && ev);
	if_assert_failed return FRAME_EVENT_IGNORED;

	/* When changing frame, vs may be NULL. See bug 525. */
	if (!doc_view->vs) return FRAME_EVENT_IGNORED;

	switch (ev->ev) {
	case EVENT_KBD:
		return frame_ev_kbd(ses, doc_view, ev);
#ifdef CONFIG_MOUSE
	case EVENT_MOUSE:
		return frame_ev_mouse(ses, doc_view, ev);
#endif /* CONFIG_MOUSE */
	default:
		return FRAME_EVENT_IGNORED;
	}
}

struct document_view *
current_frame(struct session *ses)
{
	struct document_view *doc_view = NULL;
	int current_frame_number;

	assert(ses);
	if_assert_failed return NULL;

	if (!have_location(ses)) return NULL;

	current_frame_number = cur_loc(ses)->vs.current_link;
	if (current_frame_number == -1) current_frame_number = 0;

	foreach (doc_view, ses->scrn_frames) {
		if (document_has_frames(doc_view->document)) continue;
		if (!current_frame_number--) return doc_view;
	}

	doc_view = ses->doc_view;

	assert(doc_view && doc_view->document);
	if_assert_failed return NULL;

	if (document_has_frames(doc_view->document)) return NULL;
	return doc_view;
}

static enum frame_event_status
send_to_frame(struct session *ses, struct document_view *doc_view,
	      struct term_event *ev)
{
	enum frame_event_status status;

	assert(ses && ses->tab && ses->tab->term && ev);
	if_assert_failed return FRAME_EVENT_IGNORED;

	status = frame_ev(ses, doc_view, ev);

	if (status == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);
	else
		print_screen_status(ses);

	return status;
}

#ifdef CONFIG_MOUSE
static int
do_mouse_event(struct session *ses, struct term_event *ev,
	       struct document_view *doc_view)
{
	struct term_event evv;
	struct document_view *matched = NULL, *first = doc_view;

	assert(ses && ev);
	if_assert_failed return 0;

	if (!doc_view) return 0;

	do {
		assert(doc_view && doc_view->document);
		if_assert_failed return 0;

		assertm(doc_view->document->options.box.x == doc_view->box.x
		        && doc_view->document->options.box.y == doc_view->box.y,
			"Jonas' 1.565 -> 1.566 patch sucks");
		if_assert_failed return 0;

		if (check_mouse_position(ev, &doc_view->box)) {
			matched = doc_view;
			break;
		}

		next_frame(ses, 1);
		doc_view = current_frame(ses);

	} while (doc_view != first);

	if (!matched) return 0;

	if (doc_view != first) draw_formatted(ses, 0);

	set_mouse_term_event(&evv,
			     ev->info.mouse.x - doc_view->box.x,
			     ev->info.mouse.y - doc_view->box.y,
			     ev->info.mouse.button);

	return send_to_frame(ses, doc_view, &evv);
}

static int
is_mouse_on_tab_bar(struct session *ses, struct term_event_mouse *mouse)
{
	struct terminal *term = ses->tab->term;
	int y;

	if (ses->status.show_tabs_bar_at_top) y = ses->status.show_title_bar;
	else y = term->height - 1 - !!ses->status.show_status_bar;

	return mouse->y == y;
}
/** @returns the session if event cleanup should be done or NULL if no
 * cleanup is needed. */
static struct session *
send_mouse_event(struct session *ses, struct document_view *doc_view,
		 struct term_event *ev)
{
	struct terminal *term = ses->tab->term;
	struct term_event_mouse *mouse = &ev->info.mouse;

	if (mouse->y == 0
	    && check_mouse_action(ev, B_DOWN)
	    && !check_mouse_wheel(ev)) {
		struct window *m;

		activate_bfu_technology(ses, -1);
		m = term->windows.next;
		m->handler(m, ev);

		return ses;
	}

	/* Handle tabs navigation if tabs bar is displayed. */
	if (ses->status.show_tabs_bar && is_mouse_on_tab_bar(ses, mouse)) {
		int tab_num = get_tab_number_by_xpos(term, mouse->x);
		struct window *current_tab = get_current_tab(term);

		if (check_mouse_action(ev, B_UP)) {
			if (check_mouse_button(ev, B_MIDDLE)
			    && term->current_tab == tab_num
			    && mouse->y == term->prev_mouse_event.y) {
				if (current_tab->data == ses) ses = NULL;

				close_tab(term, current_tab->data);
			}

			return ses;
		}

		if (check_mouse_button(ev, B_WHEEL_UP)) {
			switch_current_tab(ses, -1);

		} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
			switch_current_tab(ses, 1);

		} else if (tab_num != -1) {
			switch_to_tab(term, tab_num, -1);

			if (check_mouse_button(ev, B_MIDDLE)) {
				do_not_ignore_next_mouse_event(term);
			} else if (check_mouse_button(ev, B_RIGHT)) {
				tab_menu(current_tab->data, mouse->x, mouse->y, 1);
			}
		}

		return ses;
	}

	if (!do_mouse_event(ses, ev, doc_view)
	    && check_mouse_button(ev, B_RIGHT)) {
		tab_menu(ses, mouse->x, mouse->y, 0);
		return NULL;
	}

#ifdef CONFIG_LEDS
	if (ses->status.show_leds
	    && mouse->y == term->height - 1
	    && mouse->x >= term->width - LEDS_COUNT - 3) {
		menu_leds_info(term, NULL, NULL);
		return NULL;
	}
#endif

	return NULL;
}
#endif /* CONFIG_MOUSE */

static void
try_typeahead(struct session *ses, struct document_view *doc_view,
              struct term_event *ev, enum main_action action_id)
{
	switch (get_opt_int("document.browse.search.typeahead")) {
		case 0:
			return;
		case 1:
			action_id = ACT_MAIN_SEARCH_TYPEAHEAD_LINK;
			break;
		case 2:
			action_id = ACT_MAIN_SEARCH_TYPEAHEAD_TEXT;
			break;
		default:
			INTERNAL("invalid value for document.browse.search.typeahead");
	}

	search_typeahead(ses, doc_view, action_id);

	/* Cross your fingers -- I'm just asking
	 * for an infinite loop! -- Miciah */
	term_send_event(ses->tab->term, ev);
}

/** @returns the session if event cleanup should be done or NULL if no
 * cleanup is needed. */
static struct session *
send_kbd_event(struct session *ses, struct document_view *doc_view,
	       struct term_event *ev)
{
	int event;
	enum main_action action_id;

	if (doc_view && send_to_frame(ses, doc_view, ev) != FRAME_EVENT_IGNORED)
		return NULL;

	action_id = kbd_action(KEYMAP_MAIN, ev, &event);

	if (action_id == ACT_MAIN_QUIT) {
		if (check_kbd_key(ev, KBD_CTRL_C))
quit:
			action_id = ACT_MAIN_REALLY_QUIT;
	}

	switch (do_action(ses, action_id, 0)) {
		case FRAME_EVENT_SESSION_DESTROYED:
			return NULL;
		case FRAME_EVENT_IGNORED:
			break;
		case FRAME_EVENT_OK:
		case FRAME_EVENT_REFRESH:
			return ses;
	}

	if (action_id == ACT_MAIN_SCRIPTING_FUNCTION) {
#ifdef CONFIG_SCRIPTING
		trigger_event(event, ses);
#endif
		return NULL;
	}

	if (check_kbd_key(ev, KBD_CTRL_C)) goto quit;

	/* Ctrl-Alt-F should not open the File menu like Alt-f does.  */
	if (check_kbd_modifier(ev, KBD_MOD_ALT)) {
		struct window *win;

		get_kbd_modifier(ev) &= ~KBD_MOD_ALT;
		activate_bfu_technology(ses, -1);
		win = ses->tab->term->windows.next;
		win->handler(win, ev);
		if (ses->tab->term->windows.next == win) {
			deselect_mainmenu(win->term, win->data);
			print_screen_status(ses);
		}
		if (ses->tab != ses->tab->term->windows.next)
			return NULL;
		get_kbd_modifier(ev) |= KBD_MOD_ALT;

		if (doc_view
		    && get_opt_int("document.browse.accesskey"
				   ".priority") <= 0
		    && try_document_key(ses, doc_view, ev)
		       == FRAME_EVENT_REFRESH) {
			/* The document ate the key! */
			refresh_view(ses, doc_view, 0);

			return NULL;
		}

		return ses;
	}

	if (!(get_kbd_modifier(ev) & KBD_MOD_CTRL)) {
		if (doc_view) try_typeahead(ses, doc_view, ev, action_id);
	}

	return NULL;
}

void
send_event(struct session *ses, struct term_event *ev)
{
	assert(ses && ev);
	if_assert_failed return;

	if (ev->ev == EVENT_KBD) {
		struct document_view *doc_view = current_frame(ses);

		ses = send_kbd_event(ses, doc_view, ev);
	}
#ifdef CONFIG_MOUSE
	else if (ev->ev == EVENT_MOUSE) {
		struct document_view *doc_view = current_frame(ses);

		ses = send_mouse_event(ses, doc_view, ev);
	}
#endif /* CONFIG_MOUSE */

	/* @ses may disappear ie. in close_tab() */
	if (ses) ses->kbdprefix.repeat_count = 0;
}

enum frame_event_status
download_link(struct session *ses, struct document_view *doc_view,
	      action_id_T action_id)
{
	struct link *link = get_current_link(doc_view);
	void (*download)(void *ses, unsigned char *file) = start_download;

	if (!link) return FRAME_EVENT_OK;

	if (ses->download_uri) {
		done_uri(ses->download_uri);
		ses->download_uri = NULL;
	}

	switch (action_id) {
		case ACT_MAIN_LINK_DOWNLOAD_RESUME:
			download = resume_download;
		case ACT_MAIN_LINK_DOWNLOAD:
			ses->download_uri = get_link_uri(ses, doc_view, link);
			break;

		case ACT_MAIN_LINK_DOWNLOAD_IMAGE:
			if (!link->where_img) break;
			ses->download_uri = get_uri(link->where_img, 0);
			break;

		default:
			INTERNAL("I think you forgot to take your medication, mental boy!");
			return FRAME_EVENT_OK;
	}

	if (!ses->download_uri) return FRAME_EVENT_OK;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, download, NULL, 1);

	return FRAME_EVENT_OK;
}

enum frame_event_status
view_image(struct session *ses, struct document_view *doc_view, int xxxx)
{
	struct link *link = get_current_link(doc_view);

	if (link && link->where_img)
		goto_url(ses, link->where_img);

	return FRAME_EVENT_OK;
}

enum frame_event_status
save_as(struct session *ses, struct document_view *doc_view, int magic)
{
	assert(ses);
	if_assert_failed return FRAME_EVENT_OK;

	if (!have_location(ses)) return FRAME_EVENT_OK;

	if (ses->download_uri) done_uri(ses->download_uri);
	ses->download_uri = get_uri_reference(cur_loc(ses)->vs.uri);

	assert(doc_view && doc_view->document && doc_view->document->uri);
	if_assert_failed return FRAME_EVENT_OK;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, start_download, NULL, 1);

	return FRAME_EVENT_OK;
}

/*! save_formatted() passes this function as a ::cdf_callback_T to
 * create_download_finish().  */
static void
save_formatted_finish(struct terminal *term, int h,
		      void *data, enum download_flags flags)
{
	struct document *document = data;

	assert(term && document);
	if_assert_failed return;

	if (h == -1) return;
	if (dump_to_file(document, h)) {
		info_box(term, 0, N_("Save error"), ALIGN_CENTER,
			 N_("Error writing to file"));
	}
	close(h);
}

static void
save_formatted(void *data, unsigned char *file)
{
	struct session *ses = data;
	struct document_view *doc_view;

	assert(ses && ses->tab && ses->tab->term && file);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->document);
	if_assert_failed return;

	create_download_file(ses->tab->term, file, NULL,
			     DOWNLOAD_RESUME_DISABLED,
			     save_formatted_finish, doc_view->document);
}

enum frame_event_status
save_formatted_dlg(struct session *ses, struct document_view *doc_view, int xxxx)
{
	query_file(ses, doc_view->vs->uri, ses, save_formatted, NULL, 1);
	return FRAME_EVENT_OK;
}
