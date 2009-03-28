/** Tab-style (those containing real documents) windows infrastructure.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "document/document.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "main/select.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/lists.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"


struct window *
init_tab(struct terminal *term, void *data, window_handler_T handler)
{
	struct window *win = mem_calloc(1, sizeof(*win));
	struct window *pos;

	if (!win) return NULL;

	win->handler = handler;
	win->term = term;
	win->data = data;
	win->type = WINDOW_TAB;
	win->resize = 1;

	/* Insert the new tab immediately above all existing tabs in
	 * the stack of windows.  */
	foreach_tab (pos, term->windows) {
		pos = pos->prev;
		goto found_pos;
	}
	/* This is a new terminal and there are no tabs yet.  If there
	 * were a main menu already, then we'd have to place the tab
	 * above it if it were inactive, or below if it were active.  */
	assert(term->main_menu == NULL);
	pos = (struct window *) term->windows.prev;

found_pos:
	add_at_pos(pos, win);

	assert_window_stacking(term);

	return win;
}

/** Number of tabs at the terminal (in term->windows) */
NONSTATIC_INLINE int
number_of_tabs(struct terminal *term)
{
	int result = 0;
	struct window *win;

	foreach_tab (win, term->windows)
		result++;

	return result;
}

/** Number of tab */
int
get_tab_number(struct window *window)
{
	struct terminal *term = window->term;
	struct window *win;
	int current = 0;
	int num = 0;

	foreachback_tab (win, term->windows) {
		if (win == window) {
			num = current;
			break;
		}
		current++;
	}

	return num;
}

/** Get tab of an according index */
struct window *
get_tab_by_number(struct terminal *term, int num)
{
	struct window *win = NULL;

	foreachback_tab (win, term->windows) {
		if (!num) break;
		num--;
	}

	/* Ensure that the return value actually points to a struct
	 * window.  */
	assertm((LIST_OF(struct window) *) win != &term->windows,
	        "tab number out of range");
	if_assert_failed return term->windows.next;

	return win;
}

/** Returns number of the tab at @a xpos, or -1 if none. */
int
get_tab_number_by_xpos(struct terminal *term, int xpos)
{
	int num = 0;
	struct window *win = NULL;

	foreachback_tab (win, term->windows) {
		if (xpos >= win->xpos
		    && xpos < win->xpos + win->width)
			return num;
		num++;
	}

	return -1;
}

/*! If @a tabs_count > 0, then it is taken as the result of a recent
 * call to number_of_tabs() so it just uses this value. */
void
switch_to_tab(struct terminal *term, int tab, int tabs_count)
{
	if (tabs_count < 0) tabs_count = number_of_tabs(term);

	if (tabs_count > 1) {
		if (get_opt_bool("ui.tabs.wraparound")) {
			tab %= tabs_count;
			if (tab < 0) tab += tabs_count;
		} else
			int_bounds(&tab, 0, tabs_count - 1);
	} else tab = 0;

	if (tab != term->current_tab) {
		term->current_tab = tab;
		set_screen_dirty(term->screen, 0, term->height);
		redraw_terminal(term);
	}
}

void
switch_current_tab(struct session *ses, int direction)
{
	struct terminal *term = ses->tab->term;
	int tabs_count = number_of_tabs(term);
	int count;

	if (tabs_count < 2)
		return;

	count = eat_kbd_repeat_count(ses);
	if (count) direction *= count;

	switch_to_tab(term, term->current_tab + direction, tabs_count);
}

static void
really_close_tab(void *ses_)
{
	struct session *ses = ses_;
	struct terminal *term = ses->tab->term;
	struct window *current_tab = get_current_tab(term);

	if (ses->tab == current_tab) {
		int tabs_count = number_of_tabs(term);

		switch_to_tab(term, term->current_tab - 1, tabs_count - 1);
	}

	delete_window(ses->tab);
}

void
close_tab(struct terminal *term, struct session *ses)
{
	/* [gettext_accelerator_context(close_tab)] */
	int tabs_count = number_of_tabs(term);

	if (tabs_count < 2) {
		query_exit(ses);
		return;
	}

	if (!get_opt_bool("ui.tabs.confirm_close")) {
		really_close_tab(ses);
		return;
	}

	msg_box(term, NULL, 0,
		N_("Close tab"), ALIGN_CENTER,
		N_("Do you really want to close the current tab?"),
		ses, 2,
		MSG_BOX_BUTTON(N_("~Yes"), really_close_tab, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), NULL, B_ESC));
}

static void
really_close_tabs(void *ses_)
{
	struct session *ses = ses_;
	struct terminal *term = ses->tab->term;
	struct window *current_tab = get_current_tab(term);
	struct window *tab;

	foreach_tab (tab, term->windows) {
		if (tab == current_tab) continue;

		/* Update the current tab counter so assertions in the
		 * delete_window() call-chain will hold, namely the one in
		 * get_tab_by_number().  */
		if (term->current_tab > 0)
			term->current_tab--;

		tab = tab->prev;
		delete_window(tab->next);
	}

	redraw_terminal(term);
}

void
close_all_tabs_but_current(struct session *ses)
{
	/* [gettext_accelerator_context(close_all_tabs_but_current)] */
	assert(ses);
	if_assert_failed return;

	if (!get_opt_bool("ui.tabs.confirm_close")) {
		really_close_tabs(ses);
		return;
	}

	msg_box(ses->tab->term, NULL, 0,
		N_("Close tab"), ALIGN_CENTER,
		N_("Do you really want to close all except the current tab?"),
		ses, 2,
		MSG_BOX_BUTTON(N_("~Yes"), really_close_tabs, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), NULL, B_ESC));
}


void
open_uri_in_new_tab(struct session *ses, struct uri *uri, int in_background,
                    int based)
{
	assert(ses);
	/* @based means whether the current @ses location will be preloaded
	 * in the tab. */
	init_session(based ? ses : NULL, ses->tab->term, uri, in_background);
}

void
delayed_open(void *data)
{
	struct delayed_open *deo = data;

	assert(deo);
	open_uri_in_new_tab(deo->ses, deo->uri, 0, 0);
	done_uri(deo->uri);
	mem_free(deo);
}

void
open_current_link_in_new_tab(struct session *ses, int in_background)
{
	struct document_view *doc_view = current_frame(ses);
	struct uri *uri = NULL;
	struct link *link;

	if (doc_view) assert(doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (link) uri = get_link_uri(ses, doc_view, link);

	open_uri_in_new_tab(ses, uri, in_background, 1);
	if (uri) done_uri(uri);
}

void
move_current_tab(struct session *ses, int direction)
{
	struct terminal *term = ses->tab->term;
	int tabs = number_of_tabs(term);
	struct window *current_tab = get_current_tab(term);
	struct window *tab;
	int new_pos;
	int count;

	assert(ses && direction);

	count = eat_kbd_repeat_count(ses);
	if (count) direction *= count;

	new_pos = term->current_tab + direction;

	if (get_opt_bool("ui.tabs.wraparound")) {
		new_pos %= tabs;
		if (new_pos < 0) new_pos = tabs + new_pos;
	} else {
	        int_bounds(&new_pos, 0, tabs - 1);
	}
	assert(0 <= new_pos && new_pos < tabs);

	/* This protects against tabs==1 and optimizes an unusual case.  */
	if (new_pos == term->current_tab) return;

	del_from_list(current_tab);
	if (new_pos == 0) {
		tab = get_tab_by_number(term, 0);
	} else {
		tab = get_tab_by_number(term, new_pos-1)->prev;
	}
	add_at_pos(tab, current_tab);

	switch_to_tab(term, new_pos, tabs);
}
