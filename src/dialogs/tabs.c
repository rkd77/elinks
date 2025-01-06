/* tab manager dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "dialogs/tabs.h"
#include "document/document.h"
#include "document/view.h"
#include "intl/libintl.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"


struct tab_item {
	OBJECT_HEAD(struct tab_item);

	struct listbox_item *box_item;

	char *title;
	char *url;

	int i;
};

static char *tab_last_searched_title;
static char *tab_last_searched_url;

static struct tab_item *
init_tab_item(struct terminal *term, char *url, char *title, int i)
{
	struct tab_item *tab_item = (struct tab_item *)mem_calloc(1, sizeof(*tab_item));

	if (!tab_item) {
		return NULL;
	}
	tab_item->i = i;
	tab_item->title = stracpy(empty_string_or_(title));

	if (!tab_item->title) {
		mem_free(tab_item);
		return NULL;
	}
	sanitize_title(tab_item->title);
	tab_item->url = stracpy(url);

	if (!tab_item->url || !sanitize_url(tab_item->url)) {
		mem_free_if(tab_item->url);
		mem_free(tab_item->title);
		mem_free(tab_item);
		return NULL;
	}
	tab_item->box_item = add_listbox_item(&term->tab_browser, NULL, BI_LEAF, tab_item, -1);

	if (!tab_item->box_item) {
		mem_free(tab_item->url);
		mem_free(tab_item->title);
		mem_free(tab_item);
		return NULL;
	}
	object_nolock(tab_item, "tab");

	return tab_item;
}

static void
add_tab_item(struct terminal *term, char *url, char *title, int i)
{
	struct tab_item *item = init_tab_item(term, url, title, i);

	if (!item) {
		return;
	}
	add_to_history_list(&term->tabs_history, item);
}

/* Implementation of the listbox operations */

static void
lock_tab_item(struct listbox_item *item)
{
}

static void
unlock_tab_item(struct listbox_item *item)
{
}

static int
is_tab_item_used(struct listbox_item *item)
{
	return 0;
}

static char *
get_tab_item_text(struct listbox_item *box_item, struct terminal *term)
{
	struct tab_item *item = (struct tab_item *)box_item->udata;
	struct string info;

	if (get_opt_int("ui.tabs.display_type", NULL) && *item->title) {
		return stracpy(item->title);
	}

	if (!init_string(&info)) {
		return NULL;
	}
	add_string_uri_to_string(&info, item->url, URI_PUBLIC);
	return info.source;
}

static char *
get_tab_item_info(struct listbox_item *box_item, struct terminal *term)
{
	struct tab_item *item = (struct tab_item *)box_item->udata;
	struct string info;

	if (box_item->type == BI_FOLDER) return NULL;
	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("Title", term), item->title);
	add_format_to_string(&info, "\n%s: %s", _("URL", term), item->url);

	return info.source;
}

static struct listbox_item *
get_tab_item_root(struct listbox_item *box_item)
{
	return NULL;
}

static struct uri *
get_tab_item_uri(struct listbox_item *item)
{
	struct tab_item *tab_item = (struct tab_item *)item->udata;

	return get_uri(tab_item->url, URI_NONE);
}

static int
get_tab_item_number(struct listbox_item *item)
{
	struct tab_item *tab_item = (struct tab_item *)item->udata;

	return tab_item->i;
}

static int
can_delete_tab_item(struct listbox_item *item)
{
	return 0;
}

static struct listbox_ops_messages tab_messages = {
	/* cant_delete_item */
	N_("Sorry, but tabs entry \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but tabs entry \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Delete marked tabs entries"),
	/* delete_marked_items */
	N_("Delete marked tabs entries?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Delete tab entry"),
	/* delete_item; xgettext:c-format */
	N_("Delete this tab entry?"),
	/* clear_all_items_title */
	N_("Clear all tabs entries"),
	/* clear_all_items_title */
	N_("Do you really want to remove all tabs entries?"),
};

static const struct listbox_ops tab_listbox_ops = {
	lock_tab_item,
	unlock_tab_item,
	is_tab_item_used,
	get_tab_item_text,
	get_tab_item_info,
	get_tab_item_uri,
	get_tab_item_root,
	NULL, // search
	can_delete_tab_item,
	NULL, // delete
	NULL, // draw
	&tab_messages,
};

/* Searching: */

static int
tab_simple_search(struct terminal *term, char *search_url, char *search_title)
{
	struct tab_item *tab_item;

	if (!search_title || !search_url)
		return 0;

	/* Memorize last searched title */
	mem_free_set(&tab_last_searched_title, stracpy(search_title));
	if (!tab_last_searched_title) return 0;

	/* Memorize last searched url */
	mem_free_set(&tab_last_searched_url, stracpy(search_url));
	if (!tab_last_searched_url) return 0;

	if (!*search_title && !*search_url) {
		/* No search terms, make all entries visible. */
		foreach (tab_item, term->tabs_history.entries) {
			tab_item->box_item->visible = 1;
		}
		return 1;
	}

	foreach (tab_item, term->tabs_history.entries) {
		/* Make matching entries visible, hide others. */
		if ((*search_title
		     && strcasestr((const char *)tab_item->title, (const char *)search_title))
		    || (*search_url
			&& c_strcasestr((const char *)tab_item->url, (const char *)search_url))) {
			tab_item->box_item->visible = 1;
		} else {
			tab_item->box_item->visible = 0;
		}
	}
	return 1;
}


static void
tabs_search_do(void *data)
{
	struct dialog *dlg = (struct dialog *)data;
	struct terminal *term = (struct terminal *)dlg->udata2;
	struct listbox_item *item = (struct listbox_item *)term->tab_browser.root.child.next;
	struct listbox_data *box;

	if (!tab_simple_search(term, (char *)dlg->widgets[1].data, (char *)dlg->widgets[0].data)) return;
	if (list_empty(term->tab_browser.root.child)) return;

	/* Shouldn't we rather do this only for the specific listbox_data box
	 * in dlg->widget->data so only the current dialog is updated? --jonas */
	foreach (box, term->tab_browser.boxes) {
		box->top = item;
		box->sel = box->top;
	}
}

static void
launch_search_dialog(struct terminal *term, struct dialog_data *parent,
		     struct session *ses)
{
	do_edit_dialog(term, 1, N_("Search tabs"), tab_last_searched_title,
		       tab_last_searched_url, ses, parent, tabs_search_do,
		       NULL, term, EDIT_DLG_SEARCH);
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
	struct terminal *term = dlg_data->win->term;
	int *display_type;

	display_type = &get_opt_int("ui.tabs.display_type", NULL);
	*display_type = !*display_type;

	update_hierbox_browser(&term->tab_browser);

	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_goto_button(struct dialog_data *dlg_data,
                         struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct terminal *term = dlg_data->win->term;

	if (!item) return EVENT_PROCESSED;

	if (item->type == BI_LEAF) {
		int tab = get_tab_item_number(item);

		switch_to_tab(term, tab, -1);
	}
	/* Close the dialog */
	delete_window(dlg_data->win);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_close_button(struct dialog_data *dlg_data,
                         struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct session *ses = (struct session *)dlg_data->dlg->udata;
	struct terminal *term = dlg_data->win->term;

	if (!item) return EVENT_PROCESSED;

	if (item->type == BI_LEAF) {
		int num = get_tab_item_number(item);

		switch_to_tab(term, num, -1);
	}
	/* Close the dialog */
	delete_window(dlg_data->win);
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
	struct tab_item *tab_item;

	if (!box->sel) return EVENT_PROCESSED;

	tab_item = (struct tab_item *)box->sel->udata;
	if (!tab_item) return EVENT_PROCESSED;

	launch_bm_add_dialog(term, NULL, NULL,
			     tab_item->title, tab_item->url);

	return EVENT_PROCESSED;
}
#endif

/* The global tabs manager: */

static const struct hierbox_browser_button tabs_buttons[] = {
	/* [gettext_accelerator_context(.globhist_buttons)] */
	{ N_("~Goto"),           push_goto_button, 1 },
#ifdef CONFIG_BOOKMARKS
	{ N_("~Bookmark"),       push_bookmark_button, 0 },
#endif
	//{ N_("~Close"),           push_close_button, 1 },
	{ N_("~Search"),         push_search_button, 1 },
	{ N_("~Toggle display url/title"), push_toggle_display_button, 1 },
};

void
init_hierbox_tab_browser(struct terminal *term)
{
	struct hierbox_browser *tab_browser = &term->tab_browser;

	tab_browser->title = N_("Tabs manager");
	tab_browser->buttons = tabs_buttons;
	tab_browser->buttons_size = sizeof_array(tabs_buttons);
	init_list(tab_browser->boxes);
	init_list(tab_browser->dialogs);

	struct listbox_item root = {
		NULL_LIST_HEAD_EL,
		{ D_LIST_HEAD_EL(tab_browser->root.child) },
		BI_FOLDER,
		-1,
		1,
		0,
	};
	copy_struct(&tab_browser->root, &root);
	tab_browser->ops = &tab_listbox_ops;

	init_list(term->tabs_history.entries);
}

void
free_tabs_data(struct terminal *term)
{
	struct tab_item *item, *next;

	foreachsafe (item, next, term->tabs_history.entries) {
		del_from_history_list(&term->tabs_history, item);
		done_listbox_item(&term->tab_browser, item->box_item);
		mem_free_if(item->url);
		mem_free_if(item->title);
		mem_free(item);
	}
}

static void
populate_tabs_data(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	int tab_count = number_of_tabs(term);
	int i;

	for (i = 0; i < tab_count; i++) {
		struct window *tab = get_tab_by_number(term, i);
		struct session *tab_ses = (struct session *)tab->data;
		struct document_view *doc_view = tab_ses ? current_frame(tab_ses) : NULL;
		char *url = "";
		char *title = "";

		if (doc_view) {
			if (doc_view->document->title) {
				title = doc_view->document->title;
			}
			if (doc_view->vs && doc_view->vs->uri) {
				url = struri(doc_view->vs->uri);
			}
		}
		add_tab_item(term, url, title, i);
	}
	term->tab_browser.do_not_save_state = 1;
}

void
tab_manager(struct session *ses)
{
	mem_free_set(&tab_last_searched_title, NULL);
	mem_free_set(&tab_last_searched_url, NULL);
	free_tabs_data(ses->tab->term);
	populate_tabs_data(ses);
	hierbox_browser(&ses->tab->term->tab_browser, ses);
}
