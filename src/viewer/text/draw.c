/** Text mode drawing functions
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

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/options.h"
#include "document/refresh.h"
#include "document/renderer.h"
#include "document/view.h"
#include "dialogs/status.h"		/* print_screen_status() */
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"		/* current_frame() */
#include "viewer/text/vs.h"


static inline int
check_document_fragment(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;
	struct uri *uri = doc_view->vs->uri;
	int vy;
	struct string fragment;

	assert(uri->fragmentlen);

	if (!init_string(&fragment)) return -2;
	if (!add_uri_to_string(&fragment, uri, URI_FRAGMENT)) {
		done_string(&fragment);
		return -2;
	}
	decode_uri_string(&fragment);
	assert(fragment.length);
	assert(*fragment.source);

	/* Omit the leading '#' when calling find_tag. */
	vy = find_tag(document, fragment.source + 1, fragment.length - 1);
	if (vy == -1) {
		struct cache_entry *cached = document->cached;

		assert(cached);
		if (cached->incomplete || cached->cache_id != document->cache_id) {
			done_string(&fragment);
			return -2;
		}

		if (get_opt_bool("document.browse.links.missing_fragment")) {
			info_box(ses->tab->term, MSGBOX_FREE_TEXT,
			 N_("Missing fragment"), ALIGN_CENTER,
			 msg_text(ses->tab->term, N_("The requested fragment "
				  "\"%s\" doesn't exist."),
				  fragment.source));
		}
	} else {
		int_bounds(&vy, 0, document->height - 1);
	}

	done_string(&fragment);

	return vy;
}

static void
draw_frame_lines(struct terminal *term, struct frameset_desc *frameset_desc,
		 int xp, int yp, struct color_pair *colors)
{
	int y, j;

	assert(term && frameset_desc && frameset_desc->frame_desc);
	if_assert_failed return;

	y = yp - 1;
	for (j = 0; j < frameset_desc->box.height; j++) {
		int x, i;
		int height = frameset_desc->frame_desc[j * frameset_desc->box.width].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->box.width; i++) {
			int width = frameset_desc->frame_desc[i].width;

			if (i) {
				struct box box;

				set_box(&box, x, y + 1, 1, height);
				draw_box(term, &box, BORDER_SVLINE, SCREEN_ATTR_FRAME, colors);

				if (j == frameset_desc->box.height - 1)
					draw_border_cross(term, x, y + height + 1,
							  BORDER_X_UP, colors);
			} else if (j) {
				if (x >= 0)
					draw_border_cross(term, x, y,
							  BORDER_X_RIGHT, colors);
			}

			if (j) {
				struct box box;

				set_box(&box, x + 1, y, width, 1);
				draw_box(term, &box, BORDER_SHLINE, SCREEN_ATTR_FRAME, colors);

				if (i == frameset_desc->box.width - 1
				    && x + width + 1 < term->width)
					draw_border_cross(term, x + width + 1, y,
							  BORDER_X_LEFT, colors);
			} else if (i) {
				draw_border_cross(term, x, y, BORDER_X_DOWN, colors);
			}

			if (i && j)
				draw_border_char(term, x, y, BORDER_SCROSS, colors);

			x += width + 1;
		}
		y += height + 1;
	}

	y = yp - 1;
	for (j = 0; j < frameset_desc->box.height; j++) {
		int x, i;
		int pj = j * frameset_desc->box.width;
		int height = frameset_desc->frame_desc[pj].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->box.width; i++) {
			int width = frameset_desc->frame_desc[i].width;
			int p = pj + i;

			if (frameset_desc->frame_desc[p].subframe) {
				draw_frame_lines(term, frameset_desc->frame_desc[p].subframe,
						 x + 1, y + 1, colors);
			}
			x += width + 1;
		}
		y += height + 1;
	}
}

static void
draw_view_status(struct session *ses, struct document_view *doc_view, int active)
{
	struct terminal *term = ses->tab->term;

	draw_forms(term, doc_view);
	if (active) {
		draw_searched(term, doc_view);
		draw_current_link(ses, doc_view);
	}
}

/** Checks if there is a link under the cursor so it can become the current
 * highlighted link. */
static void
check_link_under_cursor(struct session *ses, struct document_view *doc_view)
{
	int x = ses->tab->x;
	int y = ses->tab->y;
	struct box *box = &doc_view->box;
	struct link *link;

	link = get_link_at_coordinates(doc_view, x - box->x, y - box->y);
	if (link && link != get_current_link(doc_view)) {
		doc_view->vs->current_link = link - doc_view->document->links;
	}
}

/** Puts the formatted document on the given terminal's screen.
 * @a active indicates whether the document is focused -- i.e.,
 * whether it is displayed in the selected frame or document. */
static void
draw_doc(struct session *ses, struct document_view *doc_view, int active)
{
	struct color_pair color;
	struct view_state *vs;
	struct terminal *term;
	struct box *box;
	int vx, vy;
	int y;

	assert(ses && ses->tab && ses->tab->term && doc_view);
	if_assert_failed return;

	box = &doc_view->box;
	term = ses->tab->term;

	/* The code in this function assumes that both width and height are
	 * bigger than 1 so we have to bail out here. */
	if (box->width < 2 || box->height < 2) return;

	if (active) {
		/* When redrawing the document after things like link menu we
		 * have to reset the cursor routing state. */
		if (ses->navigate_mode == NAVIGATE_CURSOR_ROUTING) {
			set_cursor(term, ses->tab->x, ses->tab->y, 0);
		} else {
			set_cursor(term, box->x + box->width - 1, box->y + box->height - 1, 1);
			set_window_ptr(ses->tab, box->x, box->y);
		}
	}

	color.foreground = get_opt_color("document.colors.text");
	color.background = doc_view->document->height
			 ? doc_view->document->bgcolor
			 : get_opt_color("document.colors.background");

	vs = doc_view->vs;
	if (!vs) {
		draw_box(term, box, ' ', 0, &color);
		return;
	}

	if (document_has_frames(doc_view->document)) {
	 	draw_box(term, box, ' ', 0, &color);
		draw_frame_lines(term, doc_view->document->frame_desc, box->x, box->y, &color);
		if (vs->current_link == -1)
			vs->current_link = 0;
		return;
	}

	if (ses->navigate_mode == NAVIGATE_LINKWISE) {
		check_vs(doc_view);

	} else {
		check_link_under_cursor(ses, doc_view);
	}

	if (!vs->did_fragment) {
		vy = check_document_fragment(ses, doc_view);

		if (vy != -2) vs->did_fragment = 1;
		if (vy >= 0) {
			doc_view->vs->y = vy;
			set_link(doc_view);
		}
	}
	vx = vs->x;
	vy = vs->y;
	if (doc_view->last_x != -1
	    && doc_view->last_x == vx
	    && doc_view->last_y == vy
	    && !has_search_word(doc_view)) {
		clear_link(term, doc_view);
		draw_view_status(ses, doc_view, active);
		return;
	}
	doc_view->last_x = vx;
	doc_view->last_y = vy;
	draw_box(term, box, ' ', 0, &color);
	if (!doc_view->document->height) return;

	while (vs->y >= doc_view->document->height) vs->y -= box->height;
	int_lower_bound(&vs->y, 0);
	if (vy != vs->y) {
		vy = vs->y;
		if (ses->navigate_mode == NAVIGATE_LINKWISE)
			check_vs(doc_view);
	}
	for (y = int_max(vy, 0);
	     y < int_min(doc_view->document->height, box->height + vy);
	     y++) {
		int st = int_max(vx, 0);
		int en = int_min(doc_view->document->data[y].length,
				 box->width + vx);

		if (en - st <= 0) continue;
		draw_line(term, box->x + st - vx, box->y + y - vy, en - st,
			  &doc_view->document->data[y].chars[st]);
	}
	draw_view_status(ses, doc_view, active);
	if (has_search_word(doc_view))
		doc_view->last_x = doc_view->last_y = -1;
}

static void
draw_frames(struct session *ses)
{
	struct document_view *doc_view, *current_doc_view;
	int *l;
	int n, d;

	assert(ses && ses->doc_view && ses->doc_view->document);
	if_assert_failed return;

	if (!document_has_frames(ses->doc_view->document)) return;

	n = 0;
	foreach (doc_view, ses->scrn_frames) {
	       doc_view->last_x = doc_view->last_y = -1;
	       n++;
	}
	l = &cur_loc(ses)->vs.current_link;
	*l = int_max(*l, 0) % int_max(n, 1);

	current_doc_view = current_frame(ses);
	d = 0;
	while (1) {
		int more = 0;

		foreach (doc_view, ses->scrn_frames) {
			if (doc_view->depth == d)
				draw_doc(ses, doc_view, doc_view == current_doc_view);
			else if (doc_view->depth > d)
				more = 1;
		}

		if (!more) break;
		d++;
	};
}

/** @todo @a rerender is ridiciously wound-up. */
void
draw_formatted(struct session *ses, int rerender)
{
	assert(ses && ses->tab);
	if_assert_failed return;

	if (rerender) {
		rerender--; /* Mind this when analyzing @rerender. */
		if (!(rerender & 2) && session_is_loading(ses))
			rerender |= 2;
		render_document_frames(ses, rerender);

		/* Rerendering kills the document refreshing so restart it. */
		start_document_refreshes(ses);
	}

	if (ses->tab != get_current_tab(ses->tab->term))
		return;

	if (!ses->doc_view || !ses->doc_view->document) {
		/*INTERNAL("document not formatted");*/
		struct box box;

		set_box(&box, 0, 1,
			ses->tab->term->width,
			ses->tab->term->height - 2);
		draw_box(ses->tab->term, &box, ' ', 0, NULL);
		return;
	}

	if (!ses->doc_view->vs && have_location(ses))
		ses->doc_view->vs = &cur_loc(ses)->vs;
	ses->doc_view->last_x = ses->doc_view->last_y = -1;

	refresh_view(ses, ses->doc_view, 1);
}

void
refresh_view(struct session *ses, struct document_view *doc_view, int frames)
{
	/* If refresh_view() is being called because the value of a
	 * form field has changed, @ses might not be in the current
	 * tab: consider SELECT pop-ups behind which -remote loads
	 * another tab, or setTimeout in ECMAScript.  */
	if (ses->tab == get_current_tab(ses->tab->term)) {
		draw_doc(ses, doc_view, 1);
		if (frames) draw_frames(ses);
	}
	print_screen_status(ses);
}
