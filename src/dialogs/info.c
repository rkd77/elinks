/* Info dialogs */
/* $Id: info.c,v 1.119.4.7 2005/04/06 09:11:18 jonas Exp $ */

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
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/info.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "modules/version.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#ifdef DEBUG_MEMLEAK
#include "util/memdebug.h"
#endif
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

void
menu_about(struct terminal *term, void *xxx, void *xxxx)
{
	info_box(term, MSGBOX_FREE_TEXT,
		 N_("About"), ALIGN_CENTER,
		 get_dyn_full_version(term, 1));
}

struct keys_toggle_info {
	struct terminal *term;
	int toggle;
};

static void
push_toggle_keys_display_button(void *data)
{
	struct keys_toggle_info *info = data;

	menu_keys(info->term, (void *) (long) !info->toggle, NULL);
}

void
menu_keys(struct terminal *term, void *d_, void *xxx)
{
	int d = (long) d_;

	/* We scale by main mapping because it has the most actions */
	int actions[MAIN_ACTIONS] = {
		ACT_MAIN_MENU,
		ACT_MAIN_QUIT,
		ACT_MAIN_MOVE_LINK_NEXT,
		ACT_MAIN_MOVE_LINK_PREV,
		ACT_MAIN_SCROLL_DOWN,
		ACT_MAIN_SCROLL_UP,
		ACT_MAIN_SCROLL_LEFT,
		ACT_MAIN_SCROLL_RIGHT,
		ACT_MAIN_HISTORY_MOVE_BACK,
		ACT_MAIN_GOTO_URL,
		ACT_MAIN_GOTO_URL_CURRENT,
		ACT_MAIN_DOCUMENT_INFO,
		ACT_MAIN_HEADER_INFO,
		ACT_MAIN_SEARCH,
		ACT_MAIN_SEARCH_BACK,
		ACT_MAIN_FIND_NEXT,
		ACT_MAIN_FIND_NEXT_BACK,
		ACT_MAIN_LINK_FOLLOW,
		ACT_MAIN_LINK_DOWNLOAD,
		ACT_MAIN_TOGGLE_HTML_PLAIN,

		ACT_MAIN_NONE,
	};
	struct string keys;
	struct keys_toggle_info *info;

	info = mem_calloc(1, sizeof(*info));

	if (!info || !init_string(&keys)) {
		mem_free_if(info);
		return;
	}

	info->term = term;
	info->toggle = d;

	if (info->toggle) {
		int action;
		enum keymap map;

		for (action = 0; action < MAIN_ACTIONS - 1; action++) {
			actions[action] = action + 1;
		}

		for (map = 0; map < KEYMAP_MAX; map++) {
			add_actions_to_string(&keys, actions, map, term);
			if (map + 1 < KEYMAP_MAX)
				add_to_string(&keys, "\n\n");

			/* Just a little reminder that the following code takes
			 * the easy way. */
			assert(MAIN_ACTIONS > EDIT_ACTIONS);
			assert(EDIT_ACTIONS > MENU_ACTIONS);

			if (map == KEYMAP_MAIN) {
				actions[EDIT_ACTIONS] = ACT_EDIT_NONE;
			} else if (map == KEYMAP_EDIT) {
				actions[MENU_ACTIONS] = ACT_MENU_NONE;
			}
		}
	} else {
		add_actions_to_string(&keys, (int *) actions, KEYMAP_MAIN, term);
	}

	msg_box(term, getml(info, NULL), MSGBOX_FREE_TEXT | MSGBOX_SCROLLABLE,
		N_("Keys"), ALIGN_LEFT,
		keys.source,
		info, 2,
		N_("~OK"), NULL, B_ENTER | B_ESC,
		N_("~Toggle display"), push_toggle_keys_display_button, B_ENTER);
}

void
menu_copying(struct terminal *term, void *xxx, void *xxxx)
{
	info_box(term, MSGBOX_FREE_TEXT,
		 N_("Copying"), ALIGN_CENTER,
		 msg_text(term, N_("ELinks %s\n"
		 	 "\n"
			 "(C) 1999 - 2002 Mikulas Patocka\n"
			 "(C) 2001 - 2004 Petr Baudis\n"
			 "(C) 2002 - 2005 Jonas Fonseca\n"
			 "and others\n"
			 "\n"
			 "This program is free software; you can redistribute it "
			 "and/or modify it under the terms of the GNU General Public "
			 "License as published by the Free Software Foundation, "
			 "specifically version 2 of the License."),
			 VERSION_STRING));
}


static unsigned char *
get_resource_info(struct terminal *term, void *data)
{
	struct string info;
	int terminal_count = 0, session_count = 0;
	struct terminal *terminal;
	struct session *ses;
	long val;

	if (!init_string(&info))
		return NULL;

	foreach (terminal, terminals)
		terminal_count++;

	foreach (ses, sessions)
		session_count++;

#define val_add(text) \
	add_format_to_string(&info, text, val);

	add_to_string(&info, _("Resources", term));
	add_to_string(&info, ": ");

	val = select_info(INFO_FILES);
	val_add(n_("%d handle", "%d handles", val, term));
	add_to_string(&info, ", ");

	val = select_info(INFO_TIMERS);
	val_add(n_("%d timer", "%d timers", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Connections", term));
	add_to_string(&info, ": ");

	val = connect_info(INFO_FILES);
	val_add(n_("%d connection", "%d connections", val, term));
	add_to_string(&info, ", ");

	val = connect_info(INFO_CONNECTING);
	val_add(n_("%d connecting", "%d connecting", val, term));
	add_to_string(&info, ", ");

	val = connect_info(INFO_TRANSFER);
	val_add(n_("%d transferring", "%d transferring", val, term));
	add_to_string(&info, ", ");

	val = connect_info(INFO_KEEP);
	val_add(n_("%d keepalive", "%d keepalive", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Memory cache", term));
	add_to_string(&info, ": ");

	val = cache_info(INFO_BYTES);
	val_add(n_("%d byte", "%d bytes", val, term));
	add_to_string(&info, ", ");

	val = cache_info(INFO_FILES);
	val_add(n_("%d file", "%d files", val, term));
	add_to_string(&info, ", ");

	val = cache_info(INFO_LOCKED);
	val_add(n_("%d locked", "%d locked", val, term));
	add_to_string(&info, ", ");

	val = cache_info(INFO_LOADING);
	val_add(n_("%d loading", "%d loading", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Document cache", term));
	add_to_string(&info, ": ");

	val = formatted_info(INFO_FILES);
	val_add(n_("%d formatted", "%d formatted", val, term));
	add_to_string(&info, ", ");

	val = formatted_info(INFO_LOCKED);
	val_add(n_("%d locked", "%d locked", val, term));
	add_to_string(&info, ", ");

	val = formatted_info(INFO_TIMERS);
	val_add(n_("%d refreshing", "%d refreshing", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Interlinking", term));
	add_to_string(&info, ": ");
	if (term->master)
		add_to_string(&info, _("master terminal", term));
	else
		add_to_string(&info, _("slave terminal", term));
	add_to_string(&info, ", ");

	val = terminal_count;
	val_add(n_("%d terminal", "%d terminals", val, term));
	add_to_string(&info, ", ");

	val = session_count;
	val_add(n_("%d session", "%d sessions", val, term));
	add_char_to_string(&info, '.');

#ifdef DEBUG_MEMLEAK
	add_char_to_string(&info, '\n');
	add_to_string(&info, _("Memory allocated", term));
	add_to_string(&info, ": ");

	val = mem_stats.amount;
	val_add(n_("%d byte", "%d bytes", val, term));
	add_to_string(&info, ", ");

	val = mem_stats.true_amount - mem_stats.amount;
	val_add(n_("%d byte overhead", "%d bytes overhead", val, term));

	add_format_to_string(&info, " (%0.2f%%).",
		(double) (mem_stats.true_amount - mem_stats.amount) / (double) mem_stats.amount * 100);
#endif /* DEBUG_MEMLEAK */

#undef val_add

	return info.source;
}

void
resource_info(struct terminal *term)
{
	refreshed_msg_box(term, 0, N_("Resources"), ALIGN_LEFT,
			  get_resource_info, NULL);
}
