/* Sessions action management */
/* $Id: action.c,v 1.129.4.2 2005/02/28 02:07:05 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"
#include "main.h"

#include "bookmarks/dialogs.h"
#include "cache/dialogs.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "cookies/dialogs.h"
#include "dialogs/document.h"
#include "dialogs/download.h"
#include "dialogs/exmode.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/view.h"
#include "formhist/dialogs.h"
#include "globhist/dialogs.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/action.h"
#include "sched/connection.h"
#include "sched/event.h"
#include "sched/session.h"
#include "sched/task.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"


static void
toggle_document_option(struct session *ses, unsigned char *option_name)
{
	struct option *option;
	long number;

	assert(ses && ses->doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!ses->doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	option = get_opt_rec(config_options, option_name);
	number = option->value.number + 1;

	assert(option->type == OPT_BOOL || option->type == OPT_INT);
	assert(option->max);

	/* TODO: toggle per document. --Zas */
	/* TODO: call change hooks. --jonas */
	option->value.number = (number <= option->max) ? number : option->min;

	draw_formatted(ses, 1);
}

typedef enum frame_event_status (*frame_action)(struct session *, struct document_view *, int);

static enum frame_event_status
do_frame_action(struct session *ses, struct document_view *doc_view,
		frame_action action, int magic, int jump_to_link_number)
{
	assert(ses && action);
	if_assert_failed return FRAME_EVENT_OK;

	if (!have_location(ses)) return FRAME_EVENT_OK;

	/* There is a bug for this current_frame() returning NULL for recursive
	 * framesets unfortunately bugzilla is down ATM so the number is
	 * missing. --jonas */
	if (action == set_frame && !doc_view)
		return FRAME_EVENT_OK;

	assertm(doc_view, "document not formatted");
	if_assert_failed return FRAME_EVENT_OK;

	assertm(doc_view->vs, "document view has no state");
	if_assert_failed return FRAME_EVENT_OK;

	if (jump_to_link_number && !try_jump_to_link_number(ses, doc_view))
		return FRAME_EVENT_OK;

	return action(ses, doc_view, magic);
}

static void
goto_url_action(struct session *ses,
		unsigned char *(*get_url)(struct session *, unsigned char *, size_t))
{
	unsigned char url[MAX_STR_LEN];

	if (!get_url || !get_url(ses, url, sizeof(url)))
		url[0] = 0;

	dialog_goto_url(ses, url);
}

/* This could gradually become some multiplexor / switch noodle containing
 * most if not all default handling of actions (for the main mapping) that
 * frame_ev() and/or send_event() could use as a backend. */
/* Many execution paths may lead to this code so it needs to take appropriate
 * precausions to stuff like doc_view and doc_view->vs being NULL. */
enum frame_event_status
do_action(struct session *ses, enum main_action action, int verbose)
{
	enum frame_event_status status = FRAME_EVENT_OK;
	struct terminal *term = ses->tab->term;
	struct document_view *doc_view = current_frame(ses);
	int has_vs = (doc_view && doc_view->vs);
	struct link *link = has_vs ? get_current_link(doc_view) : NULL;
	int anonymous = get_cmd_opt_bool("anonymous");

	/* Please keep in alphabetical order for now. Later we can sort by most
	 * used or something. */
	switch (action) {
		case ACT_MAIN_ABORT_CONNECTION:
			abort_loading(ses, 1);
			print_screen_status(ses);
			break;

		case ACT_MAIN_ADD_BOOKMARK:
#ifdef CONFIG_BOOKMARKS
			if (anonymous) break;
			launch_bm_add_doc_dialog(term, NULL, ses);
#endif
			break;
		case ACT_MAIN_ADD_BOOKMARK_LINK:
#ifdef CONFIG_BOOKMARKS
			if (anonymous) break;
			launch_bm_add_link_dialog(term, NULL, ses);
#endif
			break;
		case ACT_MAIN_ADD_BOOKMARK_TABS:
#ifdef CONFIG_BOOKMARKS
			if (anonymous) break;
			bookmark_terminal_tabs_dialog(term);
#endif
			break;

		case ACT_MAIN_AUTH_MANAGER:
			auth_manager(ses);
			break;

		case ACT_MAIN_BOOKMARK_MANAGER:
#ifdef CONFIG_BOOKMARKS
			bookmark_manager(ses);
#endif
			break;

		case ACT_MAIN_CACHE_MANAGER:
			cache_manager(ses);
			break;

		case ACT_MAIN_CACHE_MINIMIZE:
			shrink_memory(1);
			break;

		case ACT_MAIN_COOKIES_LOAD:
#ifdef CONFIG_COOKIES
			if (anonymous || !get_opt_bool("cookies.save")) break;
			load_cookies();
#endif
			break;

		case ACT_MAIN_COOKIE_MANAGER:
#ifdef CONFIG_COOKIES
			cookie_manager(ses);
#endif
			break;

		case ACT_MAIN_COPY_CLIPBOARD:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 copy_current_link_to_clipboard,
						 0, 1);
			break;

		case ACT_MAIN_DOCUMENT_INFO:
			document_info_dialog(ses);
			break;

		case ACT_MAIN_DOWNLOAD_MANAGER:
			download_manager(ses);
			break;

		case ACT_MAIN_EXMODE:
#ifdef CONFIG_EXMODE
			exmode_start(ses);
#endif
			break;

		case ACT_MAIN_FILE_MENU:
			activate_bfu_technology(ses, 0);
			break;

		case ACT_MAIN_FIND_NEXT:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 find_next, 1, 0);
			break;

		case ACT_MAIN_FIND_NEXT_BACK:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 find_next, -1, 0);
			break;

		case ACT_MAIN_FORGET_CREDENTIALS:
			free_auth();
			shrink_memory(1); /* flush caches */
			break;

		case ACT_MAIN_FORMHIST_MANAGER:
#ifdef CONFIG_FORMHIST
			formhist_manager(ses);
#endif
			break;

		case ACT_MAIN_FRAME_EXTERNAL_COMMAND:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 pass_uri_to_command,
						 PASS_URI_FRAME, 0);
			break;

		case ACT_MAIN_FRAME_NEXT:
			next_frame(ses, 1);
			draw_formatted(ses, 0);
			break;

		case ACT_MAIN_FRAME_MAXIMIZE:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 set_frame, 0, 0);
			break;

		case ACT_MAIN_FRAME_PREV:
			next_frame(ses, -1);
			draw_formatted(ses, 0);
			break;

		case ACT_MAIN_GOTO_URL:
			goto_url_action(ses, NULL);
			break;

		case ACT_MAIN_GOTO_URL_CURRENT:
			goto_url_action(ses, get_current_url);
			break;

		case ACT_MAIN_GOTO_URL_CURRENT_LINK:
			goto_url_action(ses, get_current_link_url);
			break;

		case ACT_MAIN_GOTO_URL_HOME:
			goto_url_home(ses);
			break;

		case ACT_MAIN_HEADER_INFO:
			protocol_header_dialog(ses);
			break;

		case ACT_MAIN_HISTORY_MANAGER:
#ifdef CONFIG_GLOBHIST
			history_manager(ses);
#endif
			break;

		case ACT_MAIN_HISTORY_MOVE_BACK:
			go_back(ses);
			break;

		case ACT_MAIN_HISTORY_MOVE_FORWARD:
			go_unback(ses);
			break;

		case ACT_MAIN_JUMP_TO_LINK:
			try_jump_to_link_number(ses, doc_view);
			break;

		case ACT_MAIN_KEYBINDING_MANAGER:
			if (anonymous) break;
			keybinding_manager(ses);
			break;

		case ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS:
			abort_background_connections();
			break;

		case ACT_MAIN_LINK_DOWNLOAD:
			if (!has_vs || anonymous) break;
			status = do_frame_action(ses, doc_view,
						 download_link,
						 action, 1);
			break;

		case ACT_MAIN_LINK_DOWNLOAD_IMAGE:
			if (!has_vs || anonymous) break;
			status = do_frame_action(ses, doc_view,
						 download_link,
						 action, 1);
			break;

		case ACT_MAIN_LINK_DOWNLOAD_RESUME:
			if (!has_vs || anonymous) break;
			status = do_frame_action(ses, doc_view,
						 download_link,
						 action, 1);
			break;

		case ACT_MAIN_LINK_EXTERNAL_COMMAND:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 pass_uri_to_command,
						 PASS_URI_LINK, 0);
			break;

		case ACT_MAIN_LINK_FOLLOW:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view, enter, 0, 1);
			break;

		case ACT_MAIN_LINK_FOLLOW_RELOAD:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view, enter, 1, 1);
			break;

		case ACT_MAIN_LINK_MENU:
			if (!try_jump_to_link_number(ses, doc_view))
				break;

			link_menu(term, NULL, ses);
			break;

		case ACT_MAIN_LUA_CONSOLE:
#ifdef CONFIG_LUA
			trigger_event_name("dialog-lua-console", ses);
#endif
			break;

		case ACT_MAIN_MARK_SET:
#ifdef CONFIG_MARKS
			ses->kbdprefix.mark = KP_MARK_SET;
			status = FRAME_EVENT_REFRESH;
#endif
			break;

		case ACT_MAIN_MARK_GOTO:
#ifdef CONFIG_MARKS
			/* TODO: Show promptly a menu (or even listbox?)
			 * with all the marks. But the next letter must
			 * still choose a mark directly! --pasky */
			ses->kbdprefix.mark = KP_MARK_GOTO;
			status = FRAME_EVENT_REFRESH;
#endif
			break;

		case ACT_MAIN_MENU:
			activate_bfu_technology(ses, -1);
			break;

		case ACT_MAIN_MOVE_CURSOR_UP:
			if (!has_vs) break;
			status = move_cursor_up(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_DOWN:
			if (!has_vs) break;
			status = move_cursor_down(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_LEFT:
			if (!has_vs) break;
			status = move_cursor_left(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_RIGHT:
			if (!has_vs) break;
			status = move_cursor_right(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_DOWN:
			if (!has_vs) break;
			status = move_link_down(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_LEFT:
			if (!has_vs) break;
			status = move_link_left(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_NEXT:
			if (!has_vs) break;
			status = move_link_next(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_PREV:
			if (!has_vs) break;
			status = move_link_prev(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_RIGHT:
			if (!has_vs) break;
			status = move_link_right(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_UP:
			if (!has_vs) break;
			status = move_link_up(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_PAGE_DOWN:
			if (!has_vs) break;
			status = move_page_down(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_PAGE_UP:
			if (!has_vs) break;
			status = move_page_up(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_DOCUMENT_START:
			if (!has_vs) break;
			status = move_document_start(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_DOCUMENT_END:
			if (!has_vs) break;
			status = move_document_end(ses, doc_view);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB:
			if (!try_jump_to_link_number(ses, doc_view))
				break;

			open_current_link_in_new_tab(ses, 0);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
			if (!try_jump_to_link_number(ses, doc_view))
				break;

			open_current_link_in_new_tab(ses, 1);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW:
			/* FIXME: Use do_frame_action(). --jonas */
			if (!has_vs) break;
			if (!try_jump_to_link_number(ses, doc_view)
			    || doc_view->vs->current_link == -1)
				break;
			open_in_new_window(term, send_open_in_new_window, ses);
			break;

		case ACT_MAIN_OPEN_NEW_TAB:
			open_uri_in_new_tab(ses, NULL, 0, 1);
			break;

		case ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND:
			open_uri_in_new_tab(ses, NULL, 1, 1);
			break;

		case ACT_MAIN_OPEN_NEW_WINDOW:
			open_in_new_window(term, send_open_new_window, ses);
			break;

		case ACT_MAIN_OPEN_OS_SHELL:
			if (anonymous) break;
			exec_shell(term);
			break;

		case ACT_MAIN_OPTIONS_MANAGER:
			if (anonymous) break;
			options_manager(ses);
			break;

		case ACT_MAIN_QUIT:
			exit_prog(ses, 1);
			break;

		case ACT_MAIN_REALLY_QUIT:
			exit_prog(ses, 0);
			break;

		case ACT_MAIN_REDRAW:
			redraw_terminal_cls(term);
			break;

		case ACT_MAIN_RELOAD:
			reload(ses, CACHE_MODE_INCREMENT);
			break;

		case ACT_MAIN_RERENDER:
			draw_formatted(ses, 2);
			break;

		case ACT_MAIN_RESET_FORM:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 reset_form, 0, 0);
			break;

		case ACT_MAIN_RESOURCE_INFO:
			resource_info(term);
			break;

		case ACT_MAIN_SAVE_AS:
			if (!has_vs || anonymous) break;
			status = do_frame_action(ses, doc_view,
						 save_as, 0, 0);
			break;

		case ACT_MAIN_SAVE_FORMATTED:
			if (!has_vs || anonymous) break;
			status = do_frame_action(ses, doc_view,
						 save_formatted_dlg,
						 0, 0);
			break;

		case ACT_MAIN_SAVE_OPTIONS:
			if (anonymous) break;
			write_config(term);
			break;

		case ACT_MAIN_SAVE_URL_AS:
			if (anonymous) break;
			save_url_as(ses);
			break;

		case ACT_MAIN_SCROLL_DOWN:
			if (!has_vs) break;
			status = scroll_down(ses, doc_view);
			break;

		case ACT_MAIN_SCROLL_LEFT:
			if (!has_vs) break;
			status = scroll_left(ses, doc_view);
			break;

		case ACT_MAIN_SCROLL_RIGHT:
			if (!has_vs) break;
			status = scroll_right(ses, doc_view);
			break;

		case ACT_MAIN_SCROLL_UP:
			if (!has_vs) break;
			status = scroll_up(ses, doc_view);
			break;

		case ACT_MAIN_SEARCH:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 search_dlg, 1, 0);
			break;

		case ACT_MAIN_SEARCH_BACK:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 search_dlg, -1, 0);
			break;

		case ACT_MAIN_SEARCH_TYPEAHEAD:
		case ACT_MAIN_SEARCH_TYPEAHEAD_LINK:
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT:
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 search_typeahead, action, 0);
			break;

		case ACT_MAIN_SHOW_TERM_OPTIONS:
			terminal_options(term, NULL, ses);
			break;

		case ACT_MAIN_SUBMIT_FORM:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 submit_form, 0, 0);
			break;

		case ACT_MAIN_SUBMIT_FORM_RELOAD:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 submit_form, 1, 0);
			break;

		case ACT_MAIN_TAB_CLOSE:
			close_tab(term, ses);
			status = FRAME_EVENT_SESSION_DESTROYED;
			break;

		case ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT:
			close_all_tabs_but_current(ses);
			break;

		case ACT_MAIN_TAB_EXTERNAL_COMMAND:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 pass_uri_to_command,
						 PASS_URI_TAB, 0);
			break;

		case ACT_MAIN_TAB_MOVE_LEFT:
			move_current_tab(ses, -1);
			break;

		case ACT_MAIN_TAB_MOVE_RIGHT:
			move_current_tab(ses, 1);
			break;

		case ACT_MAIN_TAB_MENU:
			assert(ses->tab == get_current_tab(term));

			if (ses->status.show_tabs_bar)
				tab_menu(ses, ses->tab->xpos,
					 term->height - 1
					  - ses->status.show_status_bar,
					 1);
			else
				tab_menu(ses, 0, 0, 0);

			break;

		case ACT_MAIN_TAB_NEXT:
			switch_current_tab(ses, 1);
			break;

		case ACT_MAIN_TAB_PREV:
			switch_current_tab(ses, -1);
			break;

		case ACT_MAIN_TERMINAL_RESIZE:
			resize_terminal_dialog(term);
			break;

		case ACT_MAIN_TOGGLE_CSS:
#ifdef CONFIG_CSS
			toggle_document_option(ses, "document.css.enable");
#endif
			break;

		case ACT_MAIN_TOGGLE_DISPLAY_IMAGES:
			toggle_document_option(ses, "document.browse.images.show_as_links");
			break;

		case ACT_MAIN_TOGGLE_DISPLAY_TABLES:
			toggle_document_option(ses, "document.html.display_tables");
			break;

		case ACT_MAIN_TOGGLE_DOCUMENT_COLORS:
			toggle_document_option(ses, "document.colors.use_document_colors");
			break;

		case ACT_MAIN_TOGGLE_HTML_PLAIN:
			toggle_plain_html(ses, ses->doc_view, 0);
			break;

		case ACT_MAIN_TOGGLE_NUMBERED_LINKS:
			toggle_document_option(ses, "document.browse.links.numbering");
			break;

		case ACT_MAIN_TOGGLE_PLAIN_COMPRESS_EMPTY_LINES:
			toggle_document_option(ses, "document.plain.compress_empty_lines");
			break;

		case ACT_MAIN_TOGGLE_WRAP_TEXT:
			toggle_wrap_text(ses, ses->doc_view, 0);
			break;

		case ACT_MAIN_VIEW_IMAGE:
			if (!has_vs) break;
			status = do_frame_action(ses, doc_view,
						 view_image, 0, 1);
			break;

		case ACT_MAIN_SCRIPTING_FUNCTION:
		case ACT_MAIN_NONE:
		case MAIN_ACTIONS:
		default:
			if (verbose) {
				INTERNAL("No action handling defined for '%s'.",
					 write_action(KEYMAP_MAIN, action));
			}

			status = FRAME_EVENT_IGNORED;
	}

	/* XXX: At this point the session may have been destroyed */

	if (status != FRAME_EVENT_SESSION_DESTROYED
	    && ses->insert_mode == INSERT_MODE_ON
	    && link != get_current_link(doc_view))
		ses->insert_mode = INSERT_MODE_OFF;

	if (status == FRAME_EVENT_REFRESH && doc_view)
		refresh_view(ses, doc_view, 0);

	return status;
}
