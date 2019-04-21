/* Global history dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/memory.h"
#include "util/string.h"


/* Implementation of the listbox operations */

static void
lock_globhist_item(struct listbox_item *item)
{
	object_lock((struct global_history_item *) item->udata);
}

static void
unlock_globhist_item(struct listbox_item *item)
{
	object_unlock((struct global_history_item *) item->udata);
}

static int
is_globhist_item_used(struct listbox_item *item)
{
	return is_object_used((struct global_history_item *) item->udata);
}

static unsigned char *
get_globhist_item_text(struct listbox_item *box_item, struct terminal *term)
{
	struct global_history_item *item = box_item->udata;
	struct string info;

	if (get_opt_int("document.history.global.display_type", NULL)
	    && *item->title)
		return stracpy(item->title);

	if (!init_string(&info)) return NULL;
	add_string_uri_to_string(&info, item->url, URI_PUBLIC);
	return info.source;
}

static unsigned char *
get_globhist_item_info(struct listbox_item *box_item, struct terminal *term)
{
	struct global_history_item *item = box_item->udata;
	struct string info;

	if (box_item->type == BI_FOLDER) return NULL;
	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("Title", term), item->title);
	add_format_to_string(&info, "\n%s: %s", _("URL", term), item->url);
	add_format_to_string(&info, "\n%s: %s", _("Last visit time", term),
				ctime(&item->last_visit));

	return info.source;
}

static struct listbox_item *
get_globhist_item_root(struct listbox_item *box_item)
{
	return NULL;
}

static struct uri *
get_globhist_item_uri(struct listbox_item *item)
{
	struct global_history_item *historyitem = item->udata;

	return get_uri(historyitem->url, 0);
}

static int
can_delete_globhist_item(struct listbox_item *item)
{
	return 1;
}

static void
delete_globhist_item(struct listbox_item *item, int last)
{
	struct global_history_item *historyitem = item->udata;

	assert(!is_object_used(historyitem));

	delete_global_history_item(historyitem);
}

static struct listbox_ops_messages globhist_messages = {
	/* cant_delete_item */
	N_("Sorry, but history entry \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but history entry \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Delete marked history entries"),
	/* delete_marked_items */
	N_("Delete marked history entries?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Delete history entry"),
	/* delete_item; xgettext:c-format */
	N_("Delete this history entry?"),
	/* clear_all_items_title */
	N_("Clear all history entries"),
	/* clear_all_items_title */
	N_("Do you really want to remove all history entries?"),
};

static const struct listbox_ops gh_listbox_ops = {
	lock_globhist_item,
	unlock_globhist_item,
	is_globhist_item_used,
	get_globhist_item_text,
	get_globhist_item_info,
	get_globhist_item_uri,
	get_globhist_item_root,
	NULL,
	can_delete_globhist_item,
	delete_globhist_item,
	NULL,
	&globhist_messages,
};

/* Searching: */

static void
history_search_do(void *data)
{
	struct dialog *dlg = data;
	struct listbox_item *item = globhist_browser.root.child.next;
	struct listbox_data *box;

	if (!globhist_simple_search(dlg->widgets[1].data, dlg->widgets[0].data)) return;
	if (list_empty(globhist_browser.root.child)) return;

	/* Shouldn't we rather do this only for the specific listbox_data box
	 * in dlg->widget->data so only the current dialog is updated? --jonas */
	foreach (box, globhist_browser.boxes) {
		box->top = item;
		box->sel = box->top;
	}
}

static void
launch_search_dialog(struct terminal *term, struct dialog_data *parent,
		     struct session *ses)
{
	do_edit_dialog(term, 1, N_("Search history"), gh_last_searched_title,
		       gh_last_searched_url, ses, parent, history_search_do,
		       NULL, NULL, EDIT_DLG_SEARCH);
}

static widget_handler_status_T
push_search_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	launch_search_dialog(dlg_data->win->term, dlg_data,
			     (struct session *) dlg_data->dlg->udata);
	return EVENT_PROCESSED;
}

/* Toggling: */

static widget_handler_status_T
push_toggle_display_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	int *display_type;

	display_type = &get_opt_int("document.history.global.display_type",
	                            NULL);
	*display_type = !*display_type;

	update_hierbox_browser(&globhist_browser);

	return EVENT_PROCESSED;
}

/* Bookmarking: */

#ifdef CONFIG_BOOKMARKS
static widget_handler_status_T
push_bookmark_button(struct dialog_data *dlg_data,
		     struct widget_data *some_useless_info_button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct global_history_item *historyitem;

	if (!box->sel) return EVENT_PROCESSED;

	historyitem = box->sel->udata;
	if (!historyitem) return EVENT_PROCESSED;

	launch_bm_add_dialog(term, NULL, NULL,
			     historyitem->title, historyitem->url);

	return EVENT_PROCESSED;
}
#endif

/* The global history manager: */

static const struct hierbox_browser_button globhist_buttons[] = {
	/* [gettext_accelerator_context(.globhist_buttons)] */
	{ N_("~Goto"),           push_hierbox_goto_button,   1 },
	{ N_("~Info"),           push_hierbox_info_button,   1 },
#ifdef CONFIG_BOOKMARKS
	{ N_("~Bookmark"),       push_bookmark_button,       0 },
#endif
	{ N_("~Delete"),         push_hierbox_delete_button, 0 },
	{ N_("~Search"),         push_search_button,         1 },
	{ N_("~Toggle display"), push_toggle_display_button, 1 },
	{ N_("C~lear"),          push_hierbox_clear_button,  0 },
#if 0
	/* TODO: Would this be useful? --jonas */
	{ N_("Save"),		push_save_button,            0 },
#endif
};

struct_hierbox_browser(
	globhist_browser,
	N_("Global history manager"),
	globhist_buttons,
	&gh_listbox_ops
);

void
history_manager(struct session *ses)
{
	mem_free_set(&gh_last_searched_title, NULL);
	mem_free_set(&gh_last_searched_url, NULL);
	hierbox_browser(&globhist_browser, ses);
}
