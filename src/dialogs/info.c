/* Info dialogs */

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
#include "main/select.h"
#include "main/timer.h"
#include "main/version.h"
#include "network/connection.h"
#include "session/session.h"
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
	/* [gettext_accelerator_context(menu_keys)] */
	int d = (long) d_;

	/* We scale by main mapping because it has the most actions */
	action_id_T action_ids[MAIN_ACTIONS] = {
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
		action_id_T action_id;
		enum keymap_id keymap_id;

		for (action_id = 0; action_id < MAIN_ACTIONS - 1; action_id++) {
			action_ids[action_id] = action_id + 1;
		}

		for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++) {
			add_actions_to_string(&keys, action_ids, keymap_id, term);
			if (keymap_id + 1 < KEYMAP_MAX)
				add_to_string(&keys, "\n\n");

			/* Just a little reminder that the following code takes
			 * the easy way. */
			assert((int) MAIN_ACTIONS > (int) EDIT_ACTIONS);
			assert((int) EDIT_ACTIONS > (int) MENU_ACTIONS);

			if (keymap_id == KEYMAP_MAIN) {
				action_ids[EDIT_ACTIONS] = ACT_EDIT_NONE;
			} else if (keymap_id == KEYMAP_EDIT) {
				action_ids[MENU_ACTIONS] = ACT_MENU_NONE;
			}
		}
	} else {
		add_actions_to_string(&keys, action_ids, KEYMAP_MAIN, term);
	}

	msg_box(term, getml(info, (void *) NULL), MSGBOX_FREE_TEXT | MSGBOX_SCROLLABLE,
		N_("Keys"), ALIGN_LEFT,
		keys.source,
		info, 2,
		MSG_BOX_BUTTON(N_("~OK"), NULL, B_ENTER | B_ESC),
		MSG_BOX_BUTTON(N_("~Toggle display"), push_toggle_keys_display_button, B_ENTER));
}

void
menu_copying(struct terminal *term, void *xxx, void *xxxx)
{
	info_box(term, MSGBOX_FREE_TEXT,
		 N_("Copying"), ALIGN_CENTER,
		 msg_text(term, N_("ELinks %s\n"
		 	 "\n"
			 "%s"
			  "et al.\n"
			 "\n"
			 "This program is free software; you can redistribute it "
			 "and/or modify it under the terms of the GNU General Public "
			 "License as published by the Free Software Foundation, "
			 "specifically version 2 of the License."),
			 VERSION_STRING, COPYRIGHT_STRING));
}


static unsigned char *
get_resource_info(struct terminal *term, void *data)
{
	struct string info;
	long val;
	unsigned longlong bigval;

	if (!init_string(&info))
		return NULL;

#define val_add(text) \
	add_format_to_string(&info, text, val);

	add_to_string(&info, _("Resources", term));
	add_to_string(&info, ": ");

	val = get_file_handles_count();
	val_add(n_("%ld handle", "%ld handles", val, term));
	add_to_string(&info, ", ");

	val = get_timers_count();
	val_add(n_("%ld timer", "%ld timers", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Connections", term));
	add_to_string(&info, ": ");

	val = get_connections_count();
	val_add(n_("%ld connection", "%ld connections", val, term));
	add_to_string(&info, ", ");

	val = get_connections_connecting_count();
	val_add(n_("%ld connecting", "%ld connecting", val, term));
	add_to_string(&info, ", ");

	val = get_connections_transfering_count();
	val_add(n_("%ld transferring", "%ld transferring", val, term));
	add_to_string(&info, ", ");

	val = get_keepalive_connections_count();
	val_add(n_("%ld keepalive", "%ld keepalive", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Memory cache", term));
	add_to_string(&info, ": ");

	/* What about just using Kibi/Mebi representation here? --jonas */
	bigval = get_cache_size();
	add_format_to_string(&info, n_("%ld byte", "%ld bytes", bigval, term), bigval);
	add_to_string(&info, ", ");

	val = get_cache_entry_count();
	val_add(n_("%ld file", "%ld files", val, term));
	add_to_string(&info, ", ");

	val = get_cache_entry_used_count();
	val_add(n_("%ld in use", "%ld in use", val, term));
	add_to_string(&info, ", ");

	val = get_cache_entry_loading_count();
	val_add(n_("%ld loading", "%ld loading", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Document cache", term));
	add_to_string(&info, ": ");

	val = get_format_cache_size();
	val_add(n_("%ld formatted", "%ld formatted", val, term));
	add_to_string(&info, ", ");

	val = get_format_cache_used_count();
	val_add(n_("%ld in use", "%ld in use", val, term));
	add_to_string(&info, ", ");

	val = get_format_cache_refresh_count();
	val_add(n_("%ld refreshing", "%ld refreshing", val, term));
	add_to_string(&info, ".\n");

	add_to_string(&info, _("Interlinking", term));
	add_to_string(&info, ": ");
	if (term->master)
		add_to_string(&info, _("master terminal", term));
	else
		add_to_string(&info, _("slave terminal", term));
	add_to_string(&info, ", ");

	val = list_size(&terminals);
	val_add(n_("%ld terminal", "%ld terminals", val, term));
	add_to_string(&info, ", ");

	val = list_size(&sessions);
	val_add(n_("%ld session", "%ld sessions", val, term));
	add_char_to_string(&info, '.');

#ifdef DEBUG_MEMLEAK
	add_char_to_string(&info, '\n');
	add_to_string(&info, _("Memory allocated", term));
	add_to_string(&info, ": ");

	val = mem_stats.amount;
	val_add(n_("%ld byte", "%ld bytes", val, term));
	add_to_string(&info, ", ");

	val = mem_stats.true_amount - mem_stats.amount;
	val_add(n_("%ld byte overhead", "%ld bytes overhead", val, term));

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
