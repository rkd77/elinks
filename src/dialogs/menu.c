/* Menu system */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/leds.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "document/document.h"
#include "document/view.h"
#include "dialogs/exmode.h"
#include "dialogs/info.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/main.h"
#include "main/select.h"
#include "mime/dialogs.h"
#include "mime/mime.h"
#include "network/connection.h"
#include "osdep/osdep.h"
#include "osdep/newwin.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/action.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"


/* Helper for url items in help menu. */
static void
menu_url_shortcut(struct terminal *term, void *url_, void *ses_)
{
	unsigned char *url = url_;
	struct session *ses = ses_;
	struct uri *uri = get_uri(url, 0);

	if (!uri) return;
	goto_uri(ses, uri);
	done_uri(uri);
}

static void
save_url(struct session *ses, unsigned char *url)
{
	struct document_view *doc_view;
	struct uri *uri;

	assert(ses && ses->tab && ses->tab->term && url);
	if_assert_failed return;

	if (!*url) return;

	uri = get_translated_uri(url, ses->tab->term->cwd);
	if (!uri) {
		print_error_dialog(ses, connection_state(S_BAD_URL),
				   uri, PRI_CANCEL);
		return;
	}

	if (ses->download_uri) done_uri(ses->download_uri);
	ses->download_uri = uri;

	doc_view = current_frame(ses);
	assert(doc_view && doc_view->document && doc_view->document->uri);
	if_assert_failed return;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, start_download, NULL, 1);
}

void
save_url_as(struct session *ses)
{
	input_dialog(ses->tab->term, NULL,
		     N_("Save URL"), N_("Enter URL"),
		     ses, &goto_url_history,
		     MAX_STR_LEN, "", 0, 0, NULL,
		     (void (*)(void *, unsigned char *)) save_url,
		     NULL);
}

static void
really_exit_prog(void *ses_)
{
	struct session *ses = ses_;

	register_bottom_half(destroy_terminal, ses->tab->term);
}

static inline void
dont_exit_prog(void *ses_)
{
	struct session *ses = ses_;

	ses->exit_query = 0;
}

void
query_exit(struct session *ses)
{
	/* [gettext_accelerator_context(query_exit)] */
	ses->exit_query = 1;
	msg_box(ses->tab->term, NULL, 0,
		N_("Exit ELinks"), ALIGN_CENTER,
		(ses->tab->term->next == ses->tab->term->prev && are_there_downloads())
		? N_("Do you really want to exit ELinks "
		     "(and terminate all downloads)?")
		: N_("Do you really want to exit ELinks?"),
		ses, 2,
		MSG_BOX_BUTTON(N_("~Yes"), really_exit_prog, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), dont_exit_prog, B_ESC));
}

void
exit_prog(struct session *ses, int query)
{
	assert(ses);

	/* An exit query is in progress. */
	if (ses->exit_query)
		return;

	/* Force a query if the last terminal is exiting with downloads still in
	 * progress. */
	if (query || (list_is_singleton(terminals) && are_there_downloads())) {
		query_exit(ses);
		return;
	}

	really_exit_prog(ses);
}


static void
go_historywards(struct terminal *term, void *target_, void *ses_)
{
	struct location *target = target_;
	struct session *ses = ses_;

	go_history(ses, target);
}

static struct menu_item no_hist_menu[] = {
	INIT_MENU_ITEM(N_("No history"), NULL, ACT_MAIN_NONE, NULL, NULL, NO_SELECT),
	NULL_MENU_ITEM
};

/* unhist == 0 => history
 * unhist == 1 => unhistory */
static void
history_menu_common(struct terminal *term, struct session *ses, int unhist)
{
	struct menu_item *mi = NULL;

	if (have_location(ses)) {
		struct location *loc;

		for (loc = unhist ? cur_loc(ses)->next : cur_loc(ses)->prev;
		     loc != (struct location *) &ses->history.history;
		     loc = unhist ? loc->next : loc->prev) {
			unsigned char *url;

			if (!mi) {
				mi = new_menu(FREE_LIST | FREE_TEXT | NO_INTL);
				if (!mi) return;
			}

			url = get_uri_string(loc->vs.uri, URI_PUBLIC);
			if (url) {
				add_to_menu(&mi, url, NULL, ACT_MAIN_NONE,
					    go_historywards,
				    	    (void *) loc, 0);
			}
		}
	}

	if (!mi)
		do_menu(term, no_hist_menu, ses, 0);
	else
		do_menu(term, mi, ses, 0);
}

static void
history_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;

	history_menu_common(term, ses, 0);
}

static void
unhistory_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;

	history_menu_common(term, ses, 1);
}

void
tab_menu(struct session *ses, int x, int y, int place_above_cursor)
{
	/* [gettext_accelerator_context(tab_menu)] */
	struct menu_item *menu;
	int tabs_count;
#ifdef CONFIG_BOOKMARKS
	int anonymous = get_cmd_opt_bool("anonymous");
#endif

	assert(ses && ses->tab);
	if_assert_failed return;

	tabs_count = number_of_tabs(ses->tab->term);
	menu = new_menu(FREE_LIST);
	if (!menu) return;

	add_menu_action(&menu, N_("Go ~back"), ACT_MAIN_HISTORY_MOVE_BACK);
	add_menu_action(&menu, N_("Go for~ward"), ACT_MAIN_HISTORY_MOVE_FORWARD);

	if (have_location(ses)) {
		add_menu_separator(&menu);

#ifdef CONFIG_BOOKMARKS
		if (!anonymous) {
			add_menu_action(&menu, N_("Bookm~ark document"), ACT_MAIN_ADD_BOOKMARK);
		}
#endif

		add_menu_action(&menu, N_("Toggle ~HTML/plain"), ACT_MAIN_TOGGLE_HTML_PLAIN);
		add_menu_action(&menu, N_("~Reload"), ACT_MAIN_RELOAD);

		if (ses->doc_view && document_has_frames(ses->doc_view->document)) {
			add_menu_action(&menu, N_("Frame at ~full-screen"), ACT_MAIN_FRAME_MAXIMIZE);
			add_uri_command_to_menu(&menu, PASS_URI_FRAME,
						N_("~Pass frame URI to external command"));
		}
	}

	/* Keep tab related operations below this separator */
	add_menu_separator(&menu);

	if (tabs_count > 1) {
		add_menu_action(&menu, N_("Nex~t tab"), ACT_MAIN_TAB_NEXT);
		add_menu_action(&menu, N_("Pre~v tab"), ACT_MAIN_TAB_PREV);
	}

	add_menu_action(&menu, N_("~Close tab"), ACT_MAIN_TAB_CLOSE);

	if (tabs_count > 1) {
		add_menu_action(&menu, N_("C~lose all tabs but the current"),
				ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT);
#ifdef CONFIG_BOOKMARKS
		if (!anonymous) {
			add_menu_action(&menu, N_("B~ookmark all tabs"),
					ACT_MAIN_ADD_BOOKMARK_TABS);
		}
#endif
	}

	if (have_location(ses)) {
		add_uri_command_to_menu(&menu, PASS_URI_TAB,
					N_("Pass tab URI to e~xternal command"));
	}

	/* Adjust the menu position taking the menu frame into account */
	if (place_above_cursor) {
		int i = 0;

		while (menu[i].text) i++;

		y = int_max(y - i - 1, 0);
	}

	set_window_ptr(ses->tab, x, y);

	do_menu(ses->tab->term, menu, ses, 1);
}

static void
do_submenu(struct terminal *term, void *menu_, void *ses_)
{
	struct menu_item *menu = menu_;

	do_menu(term, menu, ses_, 1);
}


static struct menu_item file_menu11[] = {
	/* [gettext_accelerator_context(.file_menu)] */
	INIT_MENU_ACTION(N_("Open new ~tab"), ACT_MAIN_OPEN_NEW_TAB),
	INIT_MENU_ACTION(N_("Open new tab in backgroun~d"), ACT_MAIN_OPEN_NEW_TAB_IN_BACKGROUND),
	INIT_MENU_ACTION(N_("~Go to URL"), ACT_MAIN_GOTO_URL),
	INIT_MENU_ACTION(N_("Go ~back"), ACT_MAIN_HISTORY_MOVE_BACK),
	INIT_MENU_ACTION(N_("Go ~forward"), ACT_MAIN_HISTORY_MOVE_FORWARD),
	INIT_MENU_ITEM(N_("~History"), NULL, ACT_MAIN_NONE, history_menu, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("~Unhistory"), NULL, ACT_MAIN_NONE, unhistory_menu, NULL, SUBMENU),
};

static struct menu_item file_menu21[] = {
	/* [gettext_accelerator_context(.file_menu)] */
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("~Save as"), ACT_MAIN_SAVE_AS),
	INIT_MENU_ACTION(N_("Save UR~L as"), ACT_MAIN_SAVE_URL_AS),
	INIT_MENU_ACTION(N_("Sa~ve formatted document"), ACT_MAIN_SAVE_FORMATTED),
#ifdef CONFIG_BOOKMARKS
	INIT_MENU_ACTION(N_("Bookm~ark document"), ACT_MAIN_ADD_BOOKMARK),
#endif
};

static struct menu_item file_menu22[] = {
	/* [gettext_accelerator_context(.file_menu)] */
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("~Kill background connections"), ACT_MAIN_KILL_BACKGROUNDED_CONNECTIONS),
	INIT_MENU_ACTION(N_("Flush all ~caches"), ACT_MAIN_CACHE_MINIMIZE),
	INIT_MENU_ACTION(N_("Resource ~info"), ACT_MAIN_RESOURCE_INFO),
	BAR_MENU_ITEM,
};

static struct menu_item file_menu3[] = {
	/* [gettext_accelerator_context(.file_menu)] */
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("E~xit"), ACT_MAIN_QUIT),
	NULL_MENU_ITEM,
};

static void
do_file_menu(struct terminal *term, void *xxx, void *ses_)
{
	/* [gettext_accelerator_context(.file_menu)] */
	struct menu_item *file_menu, *e, *f;
	int anonymous = get_cmd_opt_bool("anonymous");
	int x, o;

	file_menu = mem_alloc(sizeof(file_menu11) + sizeof(file_menu21)
			      + sizeof(file_menu22) + sizeof(file_menu3)
			      + 3 * sizeof(struct menu_item));
	if (!file_menu) return;

	e = file_menu;

	if (!anonymous
	    && !get_cmd_opt_bool("no-connect")
	    && !get_cmd_opt_bool("no-home"))
		o = can_open_in_new(term);
	else
		o = 0;

	if (o) {
		SET_MENU_ITEM(e, N_("Open ~new window"), NULL, ACT_MAIN_OPEN_NEW_WINDOW,
			      open_in_new_window, send_open_new_window,
			      (o - 1) ? SUBMENU : 0, HKS_SHOW, 0);
		e++;
	}

	memcpy(e, file_menu11, sizeof(file_menu11));
	e += sizeof_array(file_menu11);

	if (!anonymous) {
		memcpy(e, file_menu21, sizeof(file_menu21));
		e += sizeof_array(file_menu21);
	}

	memcpy(e, file_menu22, sizeof(file_menu22));
	e += sizeof_array(file_menu22);

	x = 1;
	if (!anonymous && can_open_os_shell(term->environment)) {
		SET_MENU_ITEM(e, N_("~OS shell"), NULL, ACT_MAIN_OPEN_OS_SHELL,
			      NULL, NULL, 0, HKS_SHOW, 0);
		e++;
		x = 0;
	}

	if (can_resize_window(term->environment)) {
		SET_MENU_ITEM(e, N_("Resize t~erminal"), NULL, ACT_MAIN_TERMINAL_RESIZE,
			      NULL, NULL, 0, HKS_SHOW, 0);
		e++;
		x = 0;
	}

	memcpy(e, file_menu3 + x, sizeof(file_menu3) - x * sizeof(struct menu_item));
	e += sizeof_array(file_menu3);

	for (f = file_menu; f < e; f++)
		f->flags |= FREE_LIST;

	do_menu(term, file_menu, ses_, 1);
}

static struct menu_item view_menu[] = {
	/* [gettext_accelerator_context(.view_menu)] */
	INIT_MENU_ACTION(N_("~Search"), ACT_MAIN_SEARCH),
	INIT_MENU_ACTION(N_("Search ~backward"), ACT_MAIN_SEARCH_BACK),
	INIT_MENU_ACTION(N_("Find ~next"), ACT_MAIN_FIND_NEXT),
	INIT_MENU_ACTION(N_("Find ~previous"), ACT_MAIN_FIND_NEXT_BACK),
	INIT_MENU_ACTION(N_("T~ypeahead search"), ACT_MAIN_SEARCH_TYPEAHEAD),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("Toggle ~HTML/plain"), ACT_MAIN_TOGGLE_HTML_PLAIN),
	INIT_MENU_ACTION(N_("Toggle i~mages"), ACT_MAIN_TOGGLE_DISPLAY_IMAGES),
	INIT_MENU_ACTION(N_("Toggle ~link numbering"), ACT_MAIN_TOGGLE_NUMBERED_LINKS),
	INIT_MENU_ACTION(N_("Toggle ~document colors"), ACT_MAIN_TOGGLE_DOCUMENT_COLORS),
	INIT_MENU_ACTION(N_("~Wrap text on/off"), ACT_MAIN_TOGGLE_WRAP_TEXT),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("Document ~info"), ACT_MAIN_DOCUMENT_INFO),
	INIT_MENU_ACTION(N_("H~eader info"), ACT_MAIN_HEADER_INFO),
	INIT_MENU_ACTION(N_("Rel~oad document"), ACT_MAIN_RELOAD),
	INIT_MENU_ACTION(N_("~Rerender document"), ACT_MAIN_RERENDER),
	INIT_MENU_ACTION(N_("Frame at ~full-screen"), ACT_MAIN_FRAME_MAXIMIZE),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("Nex~t tab"), ACT_MAIN_TAB_NEXT),
	INIT_MENU_ACTION(N_("Pre~v tab"), ACT_MAIN_TAB_PREV),
	INIT_MENU_ACTION(N_("~Close tab"), ACT_MAIN_TAB_CLOSE),
	NULL_MENU_ITEM
};


static struct menu_item help_menu[] = {
	/* [gettext_accelerator_context(.help_menu)] */
	INIT_MENU_ITEM(N_("~ELinks homepage"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_WEBSITE_URL, 0),
	INIT_MENU_ITEM(N_("~Documentation"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_DOC_URL, 0),
	INIT_MENU_ITEM(N_("~Keys"), NULL, ACT_MAIN_NONE, menu_keys, NULL, 0),
#ifdef CONFIG_LEDS
	INIT_MENU_ITEM(N_("LED ~indicators"), NULL, ACT_MAIN_NONE, menu_leds_info, NULL, 0),
#endif
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Bugs information"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_BUGS_URL, 0),
#ifdef CONFIG_DEBUG
	INIT_MENU_ITEM(N_("ELinks ~GITWeb"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_GITWEB_URL, 0),
#endif
	BAR_MENU_ITEM,
	INIT_MENU_ITEM(N_("~Copying"), NULL, ACT_MAIN_NONE, menu_copying, NULL, 0),
	INIT_MENU_ITEM(N_("Autho~rs"), NULL, ACT_MAIN_NONE, menu_url_shortcut, ELINKS_AUTHORS_URL, 0),
	INIT_MENU_ITEM(N_("~About"), NULL, ACT_MAIN_NONE, menu_about, NULL, 0),
	NULL_MENU_ITEM
};


static struct menu_item ext_menu[] = {
	/* [gettext_accelerator_context(.ext_menu)] */
	INIT_MENU_ITEM(N_("~Add"), NULL, ACT_MAIN_NONE, menu_add_ext, NULL, 0),
	INIT_MENU_ITEM(N_("~Modify"), NULL, ACT_MAIN_NONE, menu_list_ext, menu_add_ext, SUBMENU),
	INIT_MENU_ITEM(N_("~Delete"), NULL, ACT_MAIN_NONE, menu_list_ext, menu_del_ext, SUBMENU),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu[] = {
	/* [gettext_accelerator_context(.setup_menu)] */
#ifdef CONFIG_NLS
	INIT_MENU_ITEM(N_("~Language"), NULL, ACT_MAIN_NONE, menu_language_list, NULL, SUBMENU),
#endif
	INIT_MENU_ITEM(N_("C~haracter set"), NULL, ACT_MAIN_NONE, charset_list, NULL, SUBMENU),
	INIT_MENU_ACTION(N_("~Terminal options"), ACT_MAIN_SHOW_TERM_OPTIONS),
	INIT_MENU_ITEM(N_("File ~extensions"), NULL, ACT_MAIN_NONE, do_submenu, ext_menu, SUBMENU),
	BAR_MENU_ITEM,
	INIT_MENU_ACTION(N_("~Options manager"), ACT_MAIN_OPTIONS_MANAGER),
	INIT_MENU_ACTION(N_("~Keybinding manager"), ACT_MAIN_KEYBINDING_MANAGER),
	INIT_MENU_ACTION(N_("~Save options"), ACT_MAIN_SAVE_OPTIONS),
	NULL_MENU_ITEM
};

static struct menu_item setup_menu_anon[] = {
	/* [gettext_accelerator_context(.setup_menu)] */
	INIT_MENU_ITEM(N_("~Language"), NULL, ACT_MAIN_NONE, menu_language_list, NULL, SUBMENU),
	INIT_MENU_ITEM(N_("C~haracter set"), NULL, ACT_MAIN_NONE, charset_list, NULL, SUBMENU),
	INIT_MENU_ACTION(N_("~Terminal options"), ACT_MAIN_SHOW_TERM_OPTIONS),
	NULL_MENU_ITEM
};

static struct menu_item tools_menu[] = {
	/* [gettext_accelerator_context(.tools_menu)] */
#ifdef CONFIG_GLOBHIST
	INIT_MENU_ACTION(N_("Global ~history"), ACT_MAIN_HISTORY_MANAGER),
#endif
#ifdef CONFIG_BOOKMARKS
	INIT_MENU_ACTION(N_("~Bookmarks"), ACT_MAIN_BOOKMARK_MANAGER),
#endif
	INIT_MENU_ACTION(N_("~Cache"), ACT_MAIN_CACHE_MANAGER),
	INIT_MENU_ACTION(N_("~Downloads"), ACT_MAIN_DOWNLOAD_MANAGER),
#ifdef CONFIG_COOKIES
	INIT_MENU_ACTION(N_("Coo~kies"), ACT_MAIN_COOKIE_MANAGER),
#endif
#ifdef CONFIG_FORMHIST
	INIT_MENU_ACTION(N_("~Form history"), ACT_MAIN_FORMHIST_MANAGER),
#endif
	INIT_MENU_ACTION(N_("~Authentication"), ACT_MAIN_AUTH_MANAGER),
	NULL_MENU_ITEM
};

static void
do_setup_menu(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;

	if (!get_cmd_opt_bool("anonymous"))
		do_menu(term, setup_menu, ses, 1);
	else
		do_menu(term, setup_menu_anon, ses, 1);
}

static struct menu_item main_menu[] = {
	/* [gettext_accelerator_context(.main_menu)] */
	INIT_MENU_ITEM(N_("~File"), NULL, ACT_MAIN_NONE, do_file_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~View"), NULL, ACT_MAIN_NONE, do_submenu, view_menu, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Link"), NULL, ACT_MAIN_NONE, link_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Tools"), NULL, ACT_MAIN_NONE, do_submenu, tools_menu, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Setup"), NULL, ACT_MAIN_NONE, do_setup_menu, NULL, FREE_LIST | SUBMENU),
	INIT_MENU_ITEM(N_("~Help"), NULL, ACT_MAIN_NONE, do_submenu, help_menu, FREE_LIST | SUBMENU),
	NULL_MENU_ITEM
};

void
activate_bfu_technology(struct session *ses, int item)
{
	do_mainmenu(ses->tab->term, main_menu, ses, item);
}


void
dialog_goto_url(struct session *ses, unsigned char *url)
{
	input_dialog(ses->tab->term, NULL,
		     N_("Go to URL"), N_("Enter URL"),
		     ses, &goto_url_history,
		     MAX_STR_LEN, url, 0, 0, NULL,
		     (void (*)(void *, unsigned char *)) goto_url_with_hook,
		     NULL);
}


static INIT_INPUT_HISTORY(file_history);

void
query_file(struct session *ses, struct uri *uri, void *data,
	   void (*std)(void *, unsigned char *),
	   void (*cancel)(void *), int interactive)
{
	struct string def;

	assert(ses && uri);
	if_assert_failed return;

	/* FIXME: This ``sanity'' checking is mostly for the download code
	 * using this function. They pass ses->download_uri and we have to make
	 * sure that the connection code can download the URI. The reason we do
	 * it before is that then users won't waste time typing a filename and
	 * then discover that the URI can not be downloaded. However it might
	 * be better to introduce a set_session_download_uri() which will do
	 * the checking? --jonas */

	if (uri->protocol == PROTOCOL_UNKNOWN) {
		print_error_dialog(ses, connection_state(S_UNKNOWN_PROTOCOL),
				   uri, PRI_CANCEL);
		return;
	}

	if (get_protocol_external_handler(ses->tab->term, uri)) {
		print_error_dialog(ses, connection_state(S_EXTERNAL_PROTOCOL),
				   uri, PRI_CANCEL);
		return;
	}

	if (!init_string(&def)) return;

	add_to_string(&def, get_opt_str("document.download.directory"));
	if (def.length && !dir_sep(def.source[def.length - 1]))
		add_char_to_string(&def, '/');

	add_mime_filename_to_string(&def, uri);

	/* Remove the %-ugliness for display */
#ifdef CONFIG_UTF8
	if (ses->tab->term->utf8_cp)
		decode_uri_string(&def);
	else
#endif /* CONFIG_UTF8 */
		decode_uri_string_for_display(&def);

	if (interactive) {
		input_dialog(ses->tab->term, NULL,
			     N_("Download"), N_("Save to file"),
			     data, &file_history,
			     MAX_STR_LEN, def.source, 0, 0, check_nonempty,
			     (void (*)(void *, unsigned char *)) std,
			     (void (*)(void *)) cancel);
	} else {
		std(data, def.source);
	}

	done_string(&def);
}

void
free_history_lists(void)
{
	free_list(file_history.entries);
#ifdef CONFIG_SCRIPTING
	trigger_event_name("free-history");
#endif
}


static void
add_cmdline_bool_option(struct string *string, unsigned char *name)
{
	if (!get_cmd_opt_bool(name)) return;
	add_to_string(string, " -");
	add_to_string(string, name);
}

void
open_uri_in_new_window(struct session *ses, struct uri *uri, struct uri *referrer,
		       enum term_env_type env, enum cache_mode cache_mode,
		       enum task_type task)
{
	int ring = get_cmd_opt_int("session-ring");
	struct string parameters;
	int id;

	assert(env && ses);
	if_assert_failed return;

	id = add_session_info(ses, uri, referrer, cache_mode, task);
	if (id < 1) return;

	if (!init_string(&parameters)) return;

	add_format_to_string(&parameters, "-base-session %d", id);
	if (ring) add_format_to_string(&parameters, " -session-ring %d", ring);

	/* No URI means open new (clean) window possibly without connecting to
	 * the current master so add command line options to properly clone the
	 * current master */
	if (!uri) {
		/* Adding -touch-files will only lead to problems */
		add_cmdline_bool_option(&parameters, "localhost");
		add_cmdline_bool_option(&parameters, "no-home");
		add_cmdline_bool_option(&parameters, "no-connect");
	}

	open_new_window(ses->tab->term, program.path, env, parameters.source);
	done_string(&parameters);
}

/* Open a link in a new xterm. */
void
send_open_in_new_window(struct terminal *term, const struct open_in_new *open,
			struct session *ses)
{
	struct document_view *doc_view;
	struct link *link;
	struct uri *uri;

	assert(term && open && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link) return;

	uri = get_link_uri(ses, doc_view, link);
	if (!uri) return;

	open_uri_in_new_window(ses, uri, NULL, open->env,
			       CACHE_MODE_NORMAL, TASK_NONE);
	done_uri(uri);
}

void
send_open_new_window(struct terminal *term, const struct open_in_new *open,
		     struct session *ses)
{
	open_uri_in_new_window(ses, NULL, NULL, open->env,
			       CACHE_MODE_NORMAL, TASK_NONE);
}


void
open_in_new_window(struct terminal *term, void *func_, void *ses_)
{
	menu_func_T func = func_;
	struct session *ses = ses_;
	struct menu_item *mi;
	int posibilities;

	assert(term && ses && func);
	if_assert_failed return;

	switch (can_open_in_new(term)) {
	case 0:
		return;

	case 1:
		mi = NULL;
		break;

	default:
		mi = new_menu(FREE_LIST);
		if (!mi) return;
	}

	foreach_open_in_new (posibilities, term->environment) {
		const struct open_in_new *oi = &open_in_new[posibilities];

		if (mi == NULL) {
			func(term, (void *) oi, ses);
			return;
		}
		add_to_menu(&mi, oi->text, NULL, ACT_MAIN_NONE, func, (void *) oi, 0);
	}

	do_menu(term, mi, ses, 1);
}


void
add_new_win_to_menu(struct menu_item **mi, unsigned char *text,
		    struct terminal *term)
{
	int c = can_open_in_new(term);

	if (!c) return;

	/* The URI is saved as session info in the master and not sent to the
	 * instance in the new window so with -no-connect or -no-home enabled
	 * it is not possible to open links URIs. For -anonymous one window
	 * should be enough. */
	if (get_cmd_opt_bool("no-connect")
	    || get_cmd_opt_bool("no-home")
	    || get_cmd_opt_bool("anonymous"))
		return;

	add_to_menu(mi, text, NULL, ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW,
		    open_in_new_window,
		    send_open_in_new_window, c - 1 ? SUBMENU : 0);
}


static void
do_pass_uri_to_command(struct terminal *term, void *command_, void *xxx)
{
	unsigned char *command = command_;

	exec_on_terminal(term, command, "", TERM_EXEC_BG);
	mem_free(command);
}

/* TODO:
 * - Support for passing MIME type
 * - Merge this function with rewrite_uri(), subst_cmd(), subst_file()
 *   and subst_url(). */
static unsigned char *
format_command(unsigned char *format, struct uri *uri)
{
	struct string string;

	if (!init_string(&string)) return NULL;

	while (*format) {
		int pos = 0;

		while (format[pos] && format[pos] != '%') pos++;

		add_bytes_to_string(&string, format, pos);
		format += pos;

		if (*format != '%') continue;

		format++;
		switch (*format) {
			case 'c':
			{
				unsigned char *str = struri(uri);
				int length = get_real_uri_length(uri);

				add_shell_quoted_to_string(&string,
							   str, length);
				break;
			}
			case '%':
				add_char_to_string(&string, '%');
				break;
			default:
				add_bytes_to_string(&string, format - 1, 2);
				break;
		}
		if (*format) format++;
	}

	return string.source;
}

enum frame_event_status
pass_uri_to_command(struct session *ses, struct document_view *doc_view,
		    int which_type)
{
	LIST_OF(struct option) *tree = get_opt_tree("document.uri_passing");
	enum pass_uri_type type = which_type;
	struct menu_item *items;
	struct option *option;
	struct uri *uri;
	int commands = 0;

	switch (type) {
	case PASS_URI_FRAME:
		uri = get_uri_reference(doc_view->document->uri);
		break;

	case PASS_URI_LINK:
	{
		struct link *link = get_current_link(doc_view);

		if (!link) return FRAME_EVENT_OK;

		uri = get_link_uri(ses, doc_view, link);
		if (!uri) return FRAME_EVENT_OK;
		break;
	}
	default:
	case PASS_URI_TAB:
		uri = get_uri_reference(ses->doc_view->document->uri);
	};

	items = new_menu(FREE_LIST | FREE_TEXT | FREE_DATA | NO_INTL);
	if (!items) {
		done_uri(uri);
		return FRAME_EVENT_OK;
	}

	foreach (option, *tree) {
		unsigned char *text, *data;

		if (!strcmp(option->name, "_template_"))
			continue;

		text = stracpy(option->name);
		if (!text) continue;

		data = format_command(option->value.string, uri);
		if (!data) {
			mem_free(text);
			continue;
		}

		add_to_menu(&items, text, NULL, ACT_MAIN_NONE,
			    do_pass_uri_to_command, data, 0);
		commands++;
	}

	done_uri(uri);

	if (commands > 1) {
		do_menu(ses->tab->term, items, ses, 1);
	} else {
		if (commands == 1)
			do_pass_uri_to_command(ses->tab->term, items->data, ses);
		else
			mem_free(items->data);
		mem_free(items->text);
		mem_free(items);
	}

	return FRAME_EVENT_OK;
}

/* The caller provides the text of the menu item, so that it can
 * choose an available accelerator key. */
void
add_uri_command_to_menu(struct menu_item **mi, enum pass_uri_type type,
			unsigned char *text)
{
	LIST_OF(struct option) *tree = get_opt_tree("document.uri_passing");
	struct option *option;
	int commands = 0;
	enum menu_item_flags flags = NO_FLAG;
	action_id_T action_id;

	switch (type) {
	case PASS_URI_FRAME:
		action_id = ACT_MAIN_FRAME_EXTERNAL_COMMAND;
		break;

	case PASS_URI_LINK:
		action_id = ACT_MAIN_LINK_EXTERNAL_COMMAND;
		break;

	default:
	case PASS_URI_TAB:
		action_id = ACT_MAIN_TAB_EXTERNAL_COMMAND;
	};

	foreach (option, *tree) {
		if (!strcmp(option->name, "_template_"))
			continue;

		commands++;
		if (commands > 1) {
			flags = SUBMENU;
			break;
		}
	}

	if (commands == 0) return;

	add_to_menu(mi, text, NULL, action_id, NULL, NULL, flags);
}


/* The file completion menu always has two non selectable menu item at the
 * start. First is the 'Directory:' or 'Files:' text and then a separator. */
#define FILE_COMPLETION_MENU_OFFSET 2

static struct menu_item empty_directory_menu[] = {
	INIT_MENU_ITEM(N_("Empty directory"), NULL, ACT_MAIN_NONE, NULL, NULL, NO_SELECT),
	NULL_MENU_ITEM
};

/* Builds the file completion menu. If there is only one item it is selected
 * else the menu is launched. */
static void
complete_file_menu(struct terminal *term, int no_elevator, void *data,
		   menu_func_T file_func, menu_func_T dir_func,
		   unsigned char *dirname, unsigned char *filename)
{
	struct menu_item *menu = new_menu(FREE_LIST | NO_INTL);
	struct directory_entry *entries, *entry;
	int filenamelen = strlen(filename);
	int direntries = 0, fileentries = 0;

	if (!menu) return;

	entries = get_directory_entries(dirname, 1);
	if (!entries) {
		mem_free(menu);
		return;
	}

	for (entry = entries; entry->name; entry++) {
		unsigned char *text;
		int is_dir = (*entry->attrib == 'd');
		int is_file = (*entry->attrib == '-');

		mem_free(entry->attrib);
		if ((!is_dir && !is_file) || !file_can_read(entry->name)) {
			mem_free(entry->name);
			continue;
		}

		text = get_filename_position(entry->name);
		if (strncmp(filename, text, filenamelen)
		    || (no_elevator && !strcmp("..", text))) {
			mem_free(entry->name);
			continue;
		}

		if (is_dir) {
			if (!direntries) {
				add_to_menu(&menu, _("Directories:", term), NULL,
					    ACT_MAIN_NONE, NULL, NULL, NO_SELECT);
				add_menu_separator(&menu);
			}

			add_to_menu(&menu, text, NULL, ACT_MAIN_NONE,
				    dir_func, entry->name, FREE_DATA | SUBMENU);

			direntries++;

		} else {
			if (!fileentries) {
				if (direntries) add_menu_separator(&menu);
				add_to_menu(&menu, _("Files:", term), NULL,
					    ACT_MAIN_NONE, NULL, NULL, NO_SELECT);
				add_menu_separator(&menu);
			}

			add_to_menu(&menu, text, NULL, ACT_MAIN_NONE,
				    file_func, entry->name, FREE_DATA);

			fileentries++;

		}
	}

	mem_free(entries);
	if (direntries == 0 && fileentries == 0) {
		mem_free(menu);
		return;
	}

	/* Only one entry */
	if (direntries + fileentries == 1) {
		unsigned char *text = menu[FILE_COMPLETION_MENU_OFFSET].data;

		mem_free(menu);

		if (fileentries) {
			/* Complete what is already there */
			file_func(term, text, data);
			return;
		}

		/* For single directory entries open the lonely subdir if it is
		 * not the parent elevator. */
		if (strcmp(&text[strlen(dirname)], "..")) {
			dir_func(term, text, data);
		} else {
			do_menu(term, empty_directory_menu, NULL, 0); 			\
		}

		mem_free(text);

	} else {
		/* Start with the first directory or file entry selected */
		do_menu(term, menu, data, 0);
	}
}

/* Prepares the launching of the file completion menu by expanding the @path
 * and splitting it in directory and file name part. */
void
auto_complete_file(struct terminal *term, int no_elevator, unsigned char *path,
		   menu_func_T file_func, menu_func_T dir_func, void *data)
{
	struct uri *uri;
	unsigned char *dirname;
	unsigned char *filename;

	assert(term && data && file_func && dir_func && data);

	if (get_cmd_opt_bool("anonymous"))
		return;

	if (!*path) path = "./";

	/* Use the URI translation to handle ./ and ../ and ~/ expansion */
	uri = get_translated_uri(path, term->cwd);
	if (!uri) return;

	if (uri->protocol != PROTOCOL_FILE) {
		path = NULL;
	} else {
		path = get_uri_string(uri, URI_PATH);
	}

	done_uri(uri);
	if (!path) return;

	filename = get_filename_position(path);

	if (*filename && file_is_dir(path)) {
		filename = path + strlen(path);

	} else if (*filename && file_exists(path)) {
		/* Complete any tilde expansion */
		file_func(term, path, data);
		return;
	}

	/* Split the path into @dirname and @filename */
	dirname = path;
	path = filename;
	filename = stracpy(path);
	*path = 0;

	/* Make sure the dirname has an ending slash */
	if (!dir_sep(path[-1])) {
		unsigned char separator = *dirname;
		int dirnamelen = path - dirname;

		insert_in_string(&dirname, dirnamelen, &separator, 1);
	}

	complete_file_menu(term, no_elevator, data,
			   file_func, dir_func, dirname, filename);

	mem_free(dirname);
	mem_free(filename);
}
