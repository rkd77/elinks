/* Sessions action management */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

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
#include "main/main.h"
#include "network/connection.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "session/history.h"
#include "session/session.h"
#include "session/task.h"
#include "viewer/action.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/view.h"


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
do_action(struct session *ses, enum main_action action_id, int verbose)
{
	enum frame_event_status status = FRAME_EVENT_OK;
	struct terminal *term = ses->tab->term;
	struct document_view *doc_view = current_frame(ses);
	struct link *link = NULL;

	if (action_id == -1) goto unknown_action;

	if (doc_view && doc_view->vs) {
		if (action_prefix_is_link_number(KEYMAP_MAIN, action_id)
		    && !try_jump_to_link_number(ses, doc_view))
			goto ignore_action;

		link = get_current_link(doc_view);

	} else if (action_requires_view_state(KEYMAP_MAIN, action_id)) {
		goto ignore_action;
	}

	if (action_requires_location(KEYMAP_MAIN, action_id)
	    && !have_location(ses))
		return FRAME_EVENT_OK;

	if (action_requires_link(KEYMAP_MAIN, action_id)
	    && !link)
		goto ignore_action;

	if (action_requires_form(KEYMAP_MAIN, action_id)
	    && (!link || !link_is_form(link)))
		goto ignore_action;

	if (!action_is_anonymous_safe(KEYMAP_MAIN, action_id)
	    && get_cmd_opt_bool("anonymous"))
		goto ignore_action;

	/* Please keep in alphabetical order for now. Later we can sort by most
	 * used or something. */
	switch (action_id) {
		case ACT_MAIN_ABORT_CONNECTION:
			abort_loading(ses, 1);
			print_screen_status(ses);
			break;

		case ACT_MAIN_ADD_BOOKMARK:
#ifdef CONFIG_BOOKMARKS
			launch_bm_add_doc_dialog(term, NULL, ses);
#endif
			break;
		case ACT_MAIN_ADD_BOOKMARK_LINK:
#ifdef CONFIG_BOOKMARKS
			launch_bm_add_link_dialog(term, NULL, ses);
#endif
			break;
		case ACT_MAIN_ADD_BOOKMARK_TABS:
#ifdef CONFIG_BOOKMARKS
			bookmark_terminal_tabs_dialog(term);
#endif
			break;

		case ACT_MAIN_AUTH_MANAGER:
			auth_manager(ses);
			break;

		case ACT_MAIN_BACKSPACE_PREFIX:

			if (!ses->kbdprefix.repeat_count) break;

			/* Clear the highlighting. */
			draw_formatted(ses, 0);

			ses->kbdprefix.repeat_count /= 10;

			if (ses->kbdprefix.repeat_count)
				highlight_links_with_prefixes_that_start_with_n(
			                           term, doc_view,
			                           ses->kbdprefix.repeat_count);

			print_screen_status(ses);

			/* Keep send_event from resetting repeat_count. */
			status = FRAME_EVENT_SESSION_DESTROYED;

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
			if (!get_opt_bool("cookies.save")) break;
			load_cookies();
#endif
			break;

		case ACT_MAIN_COOKIE_MANAGER:
#ifdef CONFIG_COOKIES
			cookie_manager(ses);
#endif
			break;

		case ACT_MAIN_COPY_CLIPBOARD:
			status = copy_current_link_to_clipboard(ses, doc_view, 0);
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
			status = find_next(ses, doc_view, 1);
			break;

		case ACT_MAIN_FIND_NEXT_BACK:
			status = find_next(ses, doc_view, -1);
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
			status = pass_uri_to_command(ses, doc_view,
			                             PASS_URI_FRAME);
			break;

		case ACT_MAIN_FRAME_NEXT:
			next_frame(ses, 1);
			draw_formatted(ses, 0);
			break;

		case ACT_MAIN_FRAME_MAXIMIZE:
			status = set_frame(ses, doc_view, 0);
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
		{
			int count = int_max(1, eat_kbd_repeat_count(ses));

			go_history_by_n(ses, -count);
			break;
		}
		case ACT_MAIN_HISTORY_MOVE_FORWARD:
		{
			int count = int_max(1, eat_kbd_repeat_count(ses));

			go_history_by_n(ses, count);
			break;
		}
		case ACT_MAIN_JUMP_TO_LINK:
			break;

		case ACT_MAIN_KEYBINDING_MANAGER:
			keybinding_manager(ses);
			break;

		case ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS:
			abort_background_connections();
			break;

		case ACT_MAIN_LINK_DOWNLOAD:
		case ACT_MAIN_LINK_DOWNLOAD_IMAGE:
		case ACT_MAIN_LINK_DOWNLOAD_RESUME:
			status = download_link(ses, doc_view, action_id);
			break;

		case ACT_MAIN_LINK_EXTERNAL_COMMAND:
			status = pass_uri_to_command(ses, doc_view,
			                             PASS_URI_LINK);
			break;

		case ACT_MAIN_LINK_FOLLOW:
			status = enter(ses, doc_view, 0);
			break;

		case ACT_MAIN_LINK_FOLLOW_RELOAD:
			status = enter(ses, doc_view, 1);
			break;

		case ACT_MAIN_LINK_MENU:
			link_menu(term, NULL, ses);
			break;

		case ACT_MAIN_LINK_FORM_MENU:
			link_form_menu(ses);
			break;

		case ACT_MAIN_LUA_CONSOLE:
#ifdef CONFIG_SCRIPTING_LUA
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
			status = move_cursor_up(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_DOWN:
			status = move_cursor_down(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_LEFT:
			status = move_cursor_left(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_RIGHT:
			status = move_cursor_right(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_CURSOR_LINE_START:
			status = move_cursor_line_start(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_DOWN:
			status = move_link_down(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_DOWN_LINE:
			status = move_link_down_line(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_LEFT:
			status = move_link_left(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_LEFT_LINE:
			status = move_link_prev_line(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_NEXT:
			status = move_link_next(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_PREV:
			status = move_link_prev(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_RIGHT:
			status = move_link_right(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_RIGHT_LINE:
			status = move_link_next_line(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_UP:
			status = move_link_up(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_LINK_UP_LINE:
			status = move_link_up_line(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_PAGE_DOWN:
			status = move_page_down(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_PAGE_UP:
			status = move_page_up(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_DOCUMENT_START:
			status = move_document_start(ses, doc_view);
			break;

		case ACT_MAIN_MOVE_DOCUMENT_END:
			status = move_document_end(ses, doc_view);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB:
			open_current_link_in_new_tab(ses, 0);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
			open_current_link_in_new_tab(ses, 1);
			break;

		case ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW:
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
			exec_shell(term);
			break;

		case ACT_MAIN_OPTIONS_MANAGER:
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
			status = reset_form(ses, doc_view, 0);
			break;

		case ACT_MAIN_RESOURCE_INFO:
			resource_info(term);
			break;

		case ACT_MAIN_SAVE_AS:
			status = save_as(ses, doc_view, 0);
			break;

		case ACT_MAIN_SAVE_FORMATTED:
			status = save_formatted_dlg(ses, doc_view, 0);
			break;

		case ACT_MAIN_SAVE_OPTIONS:
			write_config(term);
			break;

		case ACT_MAIN_SAVE_URL_AS:
			save_url_as(ses);
			break;

		case ACT_MAIN_SCROLL_DOWN:
			status = scroll_down(ses, doc_view);
			break;

		case ACT_MAIN_SCROLL_LEFT:
			status = scroll_left(ses, doc_view);
			break;

		case ACT_MAIN_SCROLL_RIGHT:
			status = scroll_right(ses, doc_view);
			break;

		case ACT_MAIN_SCROLL_UP:
			status = scroll_up(ses, doc_view);
			break;

		case ACT_MAIN_SEARCH:
			status = search_dlg(ses, doc_view, 1);
			break;

		case ACT_MAIN_SEARCH_BACK:
			status = search_dlg(ses, doc_view, -1);
			break;

		case ACT_MAIN_SEARCH_TYPEAHEAD:
		case ACT_MAIN_SEARCH_TYPEAHEAD_LINK:
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT:
		case ACT_MAIN_SEARCH_TYPEAHEAD_TEXT_BACK:
			status = search_typeahead(ses, doc_view, action_id);
			break;

		case ACT_MAIN_SHOW_TERM_OPTIONS:
			terminal_options(term, NULL, ses);
			break;

		case ACT_MAIN_SUBMIT_FORM:
			status = submit_form(ses, doc_view, 0);
			break;

		case ACT_MAIN_SUBMIT_FORM_RELOAD:
			status = submit_form(ses, doc_view, 1);
			break;

		case ACT_MAIN_TAB_CLOSE:
			close_tab(term, ses);
			status = FRAME_EVENT_SESSION_DESTROYED;
			break;

		case ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT:
			close_all_tabs_but_current(ses);
			break;

		case ACT_MAIN_TAB_EXTERNAL_COMMAND:
			status = pass_uri_to_command(ses, doc_view,
			                             PASS_URI_TAB);
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

		case ACT_MAIN_TOGGLE_MOUSE:
#ifdef CONFIG_MOUSE
			toggle_mouse();
#endif
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
			status = view_image(ses, doc_view, 0);
			break;

		case ACT_MAIN_SCRIPTING_FUNCTION:
		case ACT_MAIN_NONE:
		case MAIN_ACTIONS:
		default:
unknown_action:
			if (verbose) {
				INTERNAL("No action handling defined for '%s'.",
					 get_action_name(KEYMAP_MAIN, action_id));
			}

			status = FRAME_EVENT_IGNORED;
	}

ignore_action:
	/* XXX: At this point the session may have been destroyed */

	if (status != FRAME_EVENT_SESSION_DESTROYED
	    && ses->insert_mode == INSERT_MODE_ON
	    && link != get_current_link(doc_view))
		ses->insert_mode = INSERT_MODE_OFF;

	if (status == FRAME_EVENT_REFRESH && doc_view)
		refresh_view(ses, doc_view, 0);

	return status;
}
