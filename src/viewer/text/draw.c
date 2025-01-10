/** Text mode drawing functions
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
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
#include "document/html/iframes.h"
#include "document/libdom/renderer2.h"
#include "document/options.h"
#include "document/refresh.h"
#include "document/renderer.h"
#include "document/view.h"
#include "dialogs/status.h"		/* print_screen_status() */
#include "intl/charsets.h"
#include "intl/libintl.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "terminal/draw.h"
#ifdef CONFIG_LIBSIXEL
#include "terminal/sixel.h"
#endif
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/hash.h"
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

		if (get_opt_bool("document.browse.links.missing_fragment",
		                 ses)) {
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
				struct el_box box;

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
				struct el_box box;

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
draw_iframe_lines(struct terminal *term, struct iframeset_desc *iframe_desc,
		 int xp, int yp, struct color_pair *colors)
{
	int j;

	assert(term && iframe_desc && iframe_desc->iframe_desc);
	if_assert_failed return;

	for (j = 0; j < iframe_desc->n; j++) {
		struct el_box box;

		int y = yp + iframe_desc->iframe_desc[j].box.y - 1;
		int x = xp + iframe_desc->iframe_desc[j].box.x - 1;

		int height = iframe_desc->iframe_desc[j].box.height + 1;
		int width = iframe_desc->iframe_desc[j].box.width + 1;

		set_box(&box, x, y + 1, 1, height - 1);
		draw_box(term, &box, BORDER_SVLINE, SCREEN_ATTR_FRAME, colors);

		set_box(&box, x + width, y + 1, 1, height - 1);
		draw_box(term, &box, BORDER_SVLINE, SCREEN_ATTR_FRAME, colors);

		set_box(&box, x + 1, y, width -1 , 1);
		draw_box(term, &box, BORDER_SHLINE, SCREEN_ATTR_FRAME, colors);

		set_box(&box, x + 1, y + height, width - 1, 1);
		draw_box(term, &box, BORDER_SHLINE, SCREEN_ATTR_FRAME, colors);

		draw_border_char(term, x, y, BORDER_SULCORNER, colors);
		draw_border_char(term, x, y + height, BORDER_SDLCORNER, colors);

		draw_border_char(term, x + width, y, BORDER_SURCORNER, colors);
		draw_border_char(term, x + width, y + height, BORDER_SDRCORNER, colors);
		draw_border_cross(term, x, y, BORDER_X_DOWN, colors);
	}
}


static void
draw_clipboard(struct terminal *term, struct document_view *doc_view)
{
	struct document *document = doc_view->document;
	struct color_pair *color;
	int starty, startx, endy, endx, x, y, xoffset, yoffset;

	assert(term && doc_view);
	if_assert_failed return;

	if (document->clipboard_status == CLIPBOARD_NONE) {
		return;
	}

	color = get_bfu_color(term, "clipboard");
	xoffset = doc_view->box.x - doc_view->vs->x;
	yoffset = doc_view->box.y - doc_view->vs->y;


	if (document->clipboard_box.height >= 0) {
		starty = int_max(doc_view->box.y, document->clipboard_box.y + yoffset);
		endy = int_min(doc_view->box.y + doc_view->box.height,
			document->clipboard_box.y + document->clipboard_box.height + yoffset);
	} else {
		endy = int_max(doc_view->box.y, document->clipboard_box.y + yoffset);
		starty = int_min(doc_view->box.y + doc_view->box.height,
			document->clipboard_box.y + document->clipboard_box.height + yoffset);
	}

	if (document->clipboard_box.width >= 0) {
		startx =  int_max(doc_view->box.x, document->clipboard_box.x + xoffset);
		endx = int_min(doc_view->box.x + doc_view->box.width,
			document->clipboard_box.x + document->clipboard_box.width + xoffset);
	} else {
		endx =  int_max(doc_view->box.x, document->clipboard_box.x + xoffset);
		startx = int_min(doc_view->box.x + doc_view->box.width,
			document->clipboard_box.x + document->clipboard_box.width + xoffset);
	}

	for (y = starty; y <= endy; ++y) {
		for (x = startx; x <= endx; ++x) {
			draw_char_color(term, x, y, color);
		}
	}
	doc_view->last_x = doc_view->last_y = -1;

}

static void
draw_view_status(struct session *ses, struct document_view *doc_view, int active)
{
	struct terminal *term = ses->tab->term;

	draw_forms(term, doc_view);
	if (active) {
		draw_searched(term, doc_view);
		draw_clipboard(term, doc_view);
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
	struct el_box *box = &doc_view->box;
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
	struct el_box *box, box_bg;
	struct screen_char *last = NULL;

	int vx, vy;
	int y;

	assert(ses && ses->tab && ses->tab->term && doc_view);
	if_assert_failed return;

	box = &doc_view->box;
	term = ses->tab->term;

	copy_struct(&box_bg, box);

	if (box->x == 0) {
		int margin = doc_view->document->options.margin;
		box->x += margin;

		if (box->width > 2 * margin) {
			box->width -= 2 * margin;
		}
	}

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

	color.foreground = get_opt_color("document.colors.text", ses);
	color.background = doc_view->document->height
			 ? doc_view->document->color.background
			 : get_opt_color("document.colors.background", ses);

	vs = doc_view->vs;

	if (!vs) {
		int bgchar = get_opt_int("ui.background_char", ses);
#ifdef CONFIG_UTF8
		draw_box(term, &box_bg, bgchar, 0, get_bfu_color(term, "desktop"));
#else
		draw_box(term, &box_bg, (unsigned char)bgchar, 0, get_bfu_color(term, "desktop"));
#endif
		return;
	}

	if (document_has_frames(doc_view->document)) {
		int bgchar = get_opt_int("ui.background_char", ses);
#ifdef CONFIG_UTF8
		draw_box(term, &box_bg, bgchar, 0, get_bfu_color(term, "desktop"));
#else
		draw_box(term, &box_bg, (unsigned char)bgchar, 0, get_bfu_color(term, "desktop"));
#endif
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
		if (vy == -1) {
			struct location *loc = cur_loc(ses);

			if (loc) {
				struct uri *cur_uri = loc->vs.uri;

				if (list_has_prev(ses->history.history, loc)) {
					struct uri *prev_uri = loc->prev->vs.uri;

					if (compare_uri(cur_uri, prev_uri, URI_BASE)) {
						doc_view->vs->y = doc_view->prev_y;
						set_link(doc_view);
					}
				}
			}
		}
	}
	vx = vs->x;
	vy = vs->y;
	if (doc_view->last_x != -1
	    && doc_view->last_x == vx
	    && doc_view->last_y == vy
	    && !has_search_word(doc_view)
            && !document_has_iframes(doc_view->document)) {
		clear_link(term, doc_view);
		draw_view_status(ses, doc_view, active);
		return;
	}
	doc_view->last_x = vx;
	doc_view->last_y = vy;

	int bgchar = get_opt_int("ui.background_char", ses);
#ifdef CONFIG_UTF8
	draw_box(term, &box_bg, bgchar, 0, get_bfu_color(term, "desktop"));
#else
	draw_box(term, &box_bg, (unsigned char)bgchar, 0, get_bfu_color(term, "desktop"));
#endif
	if (!doc_view->document->height) {
		return;
	}

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
		struct screen_char *first = NULL;
		int i, j;
		int last_index = 0;
		int st = int_max(vx, 0);
		int en = int_min(doc_view->document->data[y].length,
				 box->width + vx);
		int max = int_min(en, st + 200);

		if (en - st > 0) {
			draw_line(term, box->x + st - vx, box->y + y - vy,
				  en - st,
				  &doc_view->document->data[y].ch.chars[st]);

			for (i = en - 1; i >= 0; --i) {
				if (doc_view->document->data[y].ch.chars[i].data != ' ') {
					last = &doc_view->document->data[y].ch.chars[i];
					last_index = i + 1;
					break;
				}
			}
		}

		for (i = st; i < max; i++) {
			if (doc_view->document->data[y].ch.chars[i].data != ' ') {
				first = &doc_view->document->data[y].ch.chars[i];
				break;
			}
		}

		for (j = st; j < i; j++) {
			draw_space(term, box->x + j - vx, box->y + y - vy,
				   first);
		}

		for (i = last_index; i < box->width + vx; i++) {
			draw_space(term, box->x + i - vx, box->y + y - vy,
				   last);
		}
	}
#if 0
	try_to_color(term, box, doc_view->document, vx, vy);
#endif

	draw_view_status(ses, doc_view, active);
	if (has_search_word(doc_view))
		doc_view->last_x = doc_view->last_y = -1;

	if (document_has_iframes(doc_view->document)) {
		draw_iframe_lines(term, doc_view->document->iframe_desc, box->x, box->y, &color);
//		if (vs->current_link == -1)
//			vs->current_link = 0;
	}
#ifdef CONFIG_LIBSIXEL
	while (!list_empty(term->images)) {
		delete_image((struct image *)term->images.next);
	}

	if (1) {
		struct image *im;

		foreach (im, doc_view->document->images) {
			if (im->y >= vs->y + box->height) {
				continue;
			}

			if (im->y + ((im->height + term->cell_height - 1) / term->cell_height) < vs->y) {
				continue;
			}

			if (im->x >= vs->x + box->width) {
				continue;
			}

			if (im->x + ((im->width + term->cell_width - 1) / term->cell_width) < vs->x) {
				continue;
			}

			struct image *im_copy = copy_frame(im, box->width, box->height, term->cell_width, term->cell_height, vs->x, vs->y);

			if (im_copy) {
				add_to_list(term->images, im_copy);
			}
		}
	}
#endif
}

static void
draw_frames(struct session *ses)
{
	struct document_view *doc_view, *current_doc_view;
	int *l;
	int n, d;

	assert(ses && ses->doc_view && ses->doc_view->document);
	if_assert_failed {
		return;
	}

	if (!document_has_frames(ses->doc_view->document)) {
		return;
	}

	n = 0;
	foreach (doc_view, ses->scrn_frames) {
	       doc_view->last_x = doc_view->last_y = -1;
	       n++;
	}

	if (n) {
		l = &cur_loc(ses)->vs.current_link;
		*l = int_max(*l, 0) % int_max(n, 1);
	}

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

static void
draw_iframes(struct session *ses)
{
	struct document_view *doc_view, *current_doc_view;

	assert(ses && ses->doc_view && ses->doc_view->document);
	if_assert_failed {
		return;
	}

	foreach (doc_view, ses->scrn_iframes) {
	       doc_view->last_x = doc_view->last_y = -1;
	}

	current_doc_view = current_frame(ses);

	foreach (doc_view, ses->scrn_iframes) {
		draw_doc(ses, doc_view, doc_view == current_doc_view);
	}
}


/** @todo @a rerender is ridiciously wound-up. */
void
draw_formatted(struct session *ses, int rerender)
{
	assert(ses && ses->tab);
	if_assert_failed return;

	if (rerender) {
		rerender--; /* Mind this when analyzing @rerender. */
		if (!(rerender & 2) && session_is_loading(ses)) {
			rerender |= 2;
		}
		render_document_frames(ses, rerender);

		/* Rerendering kills the document refreshing so restart it. */
		start_document_refreshes(ses);
	}

	if (ses->tab != get_current_tab(ses->tab->term))
		return;

	if (!ses->doc_view || !ses->doc_view->document) {
		/*INTERNAL("document not formatted");*/
		struct el_box box;
		int bgchar = get_opt_int("ui.background_char", ses);

		set_box(&box, 0, 1,
			ses->tab->term->width,
			ses->tab->term->height - 2);
#ifdef CONFIG_UTF8
		draw_box(ses->tab->term, &box, bgchar, 0, get_bfu_color(ses->tab->term, "desktop"));
#else
		draw_box(ses->tab->term, &box, (unsigned char)bgchar, 0, get_bfu_color(ses->tab->term, "desktop"));
#endif
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
		if (doc_view->parent_doc_view) {
#ifdef CONFIG_LIBDOM
			//scan_document(doc_view->parent_doc_view);
#endif
			draw_doc(ses, doc_view->parent_doc_view, 0);
		} else {
#ifdef CONFIG_LIBDOM
			//scan_document(doc_view);
#endif
			draw_doc(ses, doc_view, 1);
		}
		if (frames) draw_frames(ses);
		draw_iframes(ses);
	}
	print_screen_status(ses);
}
