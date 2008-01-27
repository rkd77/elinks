/* Hiearchic listboxes browser dialog commons */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include "elinks.h"

#include "bfu/button.h"
#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/inpfield.h"
#include "bfu/listbox.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "session/task.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"


void
update_hierbox_browser(struct hierbox_browser *browser)
{
	struct hierbox_dialog_list_item *item;

	foreach (item, browser->dialogs) {
		redraw_from_window(item->dlg_data->win->next);
	}
}


/* Common backend for listbox adding */
struct listbox_item *
add_listbox_item(struct hierbox_browser *browser, struct listbox_item *root,
		 enum listbox_item_type type, void *data, int add_position)
{
	struct listbox_item *item;

	if (!root) {
		assertm(browser != NULL, "Nowhere to add new list box item");
		root = &browser->root;
	}

	item = mem_calloc(1, sizeof(*item));
	if (!item) return NULL;

	init_list(item->child);
	item->visible = 1;

	item->udata = data;
	item->type = type;
	item->depth = root->depth + 1;

	/* TODO: Possibility to sort by making add_position into a flag */
	if (add_position < 0)
		add_to_list_end(root->child, item);
	else
		add_to_list(root->child, item);

	if (browser) update_hierbox_browser(browser);

	return item;
}


/* Find a listbox item to replace @item. This is done by trying first to
 * traverse down then up, and if both traversals end up returning the @item
 * (that is, it is the last item in the box), return NULL. */
static inline struct listbox_item *
replace_listbox_item(struct listbox_item *item, struct listbox_data *data)
{
	struct listbox_item *new_item;

	new_item = traverse_listbox_items_list(item, data, 1, 1, NULL, NULL);
	if (item != new_item) return new_item;

	new_item = traverse_listbox_items_list(item, data, -1, 1, NULL, NULL);
	return (item == new_item) ? NULL : new_item;
}

void
done_listbox_item(struct hierbox_browser *browser, struct listbox_item *item)
{
	struct listbox_data *box_data;

	assert(item && list_empty(item->child));
	if_assert_failed return;

	/* The option dialog needs this test */
	if (item->next) {
		/* If we are removing the top or the selected box
		 * we have to figure out a replacement. */

		foreach (box_data, browser->boxes) {
			if (box_data->sel == item)
				box_data->sel = replace_listbox_item(item,
				                                     box_data);

			if (box_data->top == item)
				box_data->top = replace_listbox_item(item,
				                                     box_data);
		}

		del_from_list(item);

		update_hierbox_browser(browser);
	}

	mem_free(item);
}


static void
recursively_set_expanded(struct listbox_item *item, int expanded)
{
	struct listbox_item *child;

	if (item->type != BI_FOLDER)
		return;

	item->expanded = expanded;

	foreach (child, item->child)
		recursively_set_expanded(child, expanded);
}

static widget_handler_status_T
hierbox_ev_kbd(struct dialog_data *dlg_data)
{
	struct hierbox_browser *browser = dlg_data->dlg->udata2;
	struct widget_data *widget_data = dlg_data->widgets_data;
	struct widget *widget = widget_data->widget;
	struct listbox_data *box;
	struct listbox_item *selected;
	enum menu_action action_id;
	struct term_event *ev = dlg_data->term_event;

	/* Check if listbox has something to say to this */
	if (widget->ops->kbd
	    && widget->ops->kbd(dlg_data, widget_data)
	       == EVENT_PROCESSED)
		return EVENT_PROCESSED;

	box = get_dlg_listbox_data(dlg_data);
	selected = box->sel;
	action_id = kbd_action(KEYMAP_MENU, ev, NULL);

	switch (action_id) {
	case ACT_MENU_SELECT:
		if (!selected) return EVENT_PROCESSED;
		if (selected->type != BI_FOLDER)
			return EVENT_NOT_PROCESSED;
		selected->expanded = !selected->expanded;
		break;

	case ACT_MENU_UNEXPAND:
		/* Recursively unexpand all folders */
		if (!selected) return EVENT_PROCESSED;

		/* Special trick: if the folder is already
		 * folded, jump to the parent folder, so the
		 * next time when user presses the key, the
		 * whole parent folder will be closed. */
		if (list_empty(selected->child)
		    || !selected->expanded) {
			struct listbox_item *root;

			root = box->ops->get_root(selected);
			if (root) {
				listbox_sel(widget_data, root);
			}

		} else if (selected->type == BI_FOLDER) {
			recursively_set_expanded(selected, 0);
		}
		break;

	case ACT_MENU_EXPAND:
		/* Recursively expand all folders */

		if (!selected || selected->type != BI_FOLDER)
			return EVENT_PROCESSED;

		recursively_set_expanded(selected, 1);
		break;

	case ACT_MENU_SEARCH:
		if (!box->ops->match)
			return EVENT_NOT_PROCESSED;

		push_hierbox_search_button(dlg_data, NULL);
		return EVENT_PROCESSED;

	default:
		return EVENT_NOT_PROCESSED;

	}

	if (browser->expansion_callback)
		browser->expansion_callback();

	display_widget(dlg_data, widget_data);

	return EVENT_PROCESSED;
}

static widget_handler_status_T
hierbox_ev_init(struct dialog_data *dlg_data)
{
	struct hierbox_browser *browser = dlg_data->dlg->udata2;
	struct hierbox_dialog_list_item *item;
	struct listbox_item *litem;

	/* If we fail here it only means automatic updating
	 * will not be possible so no need to panic. */
	item = mem_alloc(sizeof(*item));
	if (item) {
		item->dlg_data = dlg_data;
		add_to_list(browser->dialogs, item);
	}

	foreach (litem, browser->root.child) {
		litem->visible = 1;
	}

	/* Return this so that the generic dialog code will run and initialise
	 * the widgets and stuff. */
	return EVENT_NOT_PROCESSED;
}

static widget_handler_status_T
hierbox_ev_abort(struct dialog_data *dlg_data)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct hierbox_browser *browser = dlg_data->dlg->udata2;
	struct hierbox_dialog_list_item *item;

	/* Save state and delete the box structure */
	if (!browser->do_not_save_state)
		copy_struct(&browser->box_data, box);
	del_from_list(box);

	/* Delete the dialog list entry */
	foreach (item, browser->dialogs) {
		if (item->dlg_data == dlg_data) {
			del_from_list(item);
			mem_free(item);
			break;
		}
	}

	/* Return this so that the generic dialog code will run and initialise
	 * the widgets and stuff. */
	return EVENT_NOT_PROCESSED;
}


/* We install own dialog event handler, so that we can give the listbox widget
 * an early chance to catch the event. Basically, the listbox widget is itself
 * unselectable, instead one of the buttons below is always active. So, we
 * always first let the listbox catch the keypress and handle it, and if it
 * doesn't care, we pass it on to the button. */
static widget_handler_status_T
hierbox_dialog_event_handler(struct dialog_data *dlg_data)
{
	struct term_event *ev = dlg_data->term_event;

	switch (ev->ev) {
		case EVENT_KBD:
			return hierbox_ev_kbd(dlg_data);

		case EVENT_INIT:
			return hierbox_ev_init(dlg_data);

		case EVENT_RESIZE:
		case EVENT_REDRAW:
		case EVENT_MOUSE:
			return EVENT_NOT_PROCESSED;

		case EVENT_ABORT:
			return hierbox_ev_abort(dlg_data);
	}

	return EVENT_NOT_PROCESSED;
}


struct dialog_data *
hierbox_browser(struct hierbox_browser *browser, struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct listbox_data *listbox_data;
	struct dialog *dlg;
	int button = browser->buttons_size + 2;
	int anonymous = get_cmd_opt_bool("anonymous");

	assert(ses);

	dlg = calloc_dialog(button, sizeof(*listbox_data));
	if (!dlg) return NULL;

	listbox_data = (struct listbox_data *) get_dialog_offset(dlg, button);

	dlg->title = _(browser->title, term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;
	dlg->layout.padding_top = 1;
	dlg->handle_event = hierbox_dialog_event_handler;
	dlg->udata = ses;
	dlg->udata2 = browser;

	add_dlg_listbox(dlg, listbox_data);

	for (button = 0; button < browser->buttons_size; button++) {
		const struct hierbox_browser_button *but = &browser->buttons[button];

		/* Skip buttons that should not be displayed in anonymous mode */
		if (anonymous && !but->anonymous) {
			anonymous++;
			continue;
		}

		add_dlg_button(dlg, _(but->label, term), B_ENTER, but->handler, NULL);
	}

	add_dlg_button(dlg, _("Close", term), B_ESC, cancel_dialog, NULL);

	/* @anonymous was initially 1 if we are running in anonymous mode so we
	 * have to subtract one. */
	add_dlg_end(dlg, button + 2 - (anonymous ? anonymous - 1 : 0));

	return do_dialog(term, dlg, getml(dlg, (void *) NULL));
}


/* Action info management */

static int
scan_for_marks(struct listbox_item *item, void *info_, int *offset)
{
	if (item->marked) {
		struct listbox_context *context = info_;

		context->item = NULL;
		*offset = 0;
	}

	return 0;
}

static int
scan_for_used(struct listbox_item *item, void *info_, int *offset)
{
	struct listbox_context *context = info_;

	if (context->box->ops->is_used(item)) {
		context->item = item;
		*offset = 0;
	}

	return 0;
}


static struct listbox_context *
init_listbox_context(struct listbox_data *box, struct terminal *term,
		     struct listbox_item *item,
		     int (*scanner)(struct listbox_item *, void *, int *))
{
	struct listbox_context *context;

	context = mem_calloc(1, sizeof(*context));
	if (!context) return NULL;

	context->item = item;
	context->term = term;
	context->box = box;

	if (!scanner) return context;

	/* Look if it wouldn't be more interesting to blast off the marked
	 * item. */
	assert(!list_empty(*box->items));
	traverse_listbox_items_list(box->items->next, box, 0, 0,
				    scanner, context);

	return context;
}

static void
done_listbox_context(void *context_)
{
	struct listbox_context *context = context_;

	if (context->item)
		context->box->ops->unlock(context->item);
}


/* Info action */

widget_handler_status_T
push_hierbox_info_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	/* [gettext_accelerator_context(push_hierbox_info_button)] */
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;
	unsigned char *msg;

	if (!item) return EVENT_PROCESSED;

	assert(box->ops);

	context = init_listbox_context(box, term, item, NULL);
	if (!context) return EVENT_PROCESSED;

	msg = box->ops->get_info(item, term);
	if (!msg) {
		mem_free(context);
		if (item->type == BI_FOLDER) {
			info_box(term, 0, N_("Info"), ALIGN_CENTER,
				 N_("Press space to expand this folder."));
		}
		return EVENT_PROCESSED;
	}

	box->ops->lock(item);

	msg_box(term, getml(context, (void *) NULL), MSGBOX_FREE_TEXT /* | MSGBOX_SCROLLABLE */,
		N_("Info"), ALIGN_LEFT,
		msg,
		context, 1,
		MSG_BOX_BUTTON(N_("~OK"), done_listbox_context, B_ESC | B_ENTER));

	return EVENT_PROCESSED;
}


/* Goto action */

static void recursively_goto_each_listbox(struct session *ses,
                                          struct listbox_item *root,
                                          struct listbox_data *box);

static void
recursively_goto_listbox(struct session *ses, struct listbox_item *item,
			 struct listbox_data *box)
{
	if (item->type == BI_FOLDER) {
		recursively_goto_each_listbox(ses, item, box);
		return;

	} else if (item->type == BI_LEAF) {
		struct uri *uri = box->ops->get_uri(item);

		if (!uri) return;

		open_uri_in_new_tab(ses, uri, 1, 0);
		done_uri(uri);
	}
}

static void
recursively_goto_each_listbox(struct session *ses, struct listbox_item *root,
			 struct listbox_data *box)
{
	struct listbox_item *item;

	foreach (item, root->child) {
		recursively_goto_listbox(ses, item, box);
	}
}

static int
goto_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *context = data_;

	if (item->marked) {
		struct session *ses = context->dlg_data->dlg->udata;
		struct listbox_data *box = context->box;

		recursively_goto_listbox(ses, item, box);
	}

	return 0;
}

widget_handler_status_T
push_hierbox_goto_button(struct dialog_data *dlg_data,
			 struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct session *ses = dlg_data->dlg->udata;
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;

	if (!item) return EVENT_PROCESSED;

	context = init_listbox_context(box, term, item, scan_for_marks);
	if (!context) return EVENT_PROCESSED;

	if (!context->item) {
		context->dlg_data = dlg_data;
		traverse_listbox_items_list(context->box->items->next,
					    context->box, 0, 0,
					    goto_marked, context);

	} else if (item->type == BI_FOLDER) {
		recursively_goto_each_listbox(ses, item, box);

	} else if (item->type == BI_LEAF) {
		struct uri *uri = box->ops->get_uri(item);

		if (uri) {
			goto_uri(ses, uri);
			done_uri(uri);
		}

	} else {
		mem_free(context);
		return EVENT_PROCESSED;
	}

	mem_free(context);

	/* Close the dialog */
	delete_window(dlg_data->win);
	return EVENT_PROCESSED;
}


/* Delete action */

enum delete_error {
	DELETE_IMPOSSIBLE = 0,
	DELETE_LOCKED,
	DELETE_ERRORS,
};

static const struct listbox_ops_messages default_listbox_ops_messages = {
	/* cant_delete_item */
	N_("Sorry, but the item \"%s\" cannot be deleted."),

	/* cant_delete_used_item */
	N_("Sorry, but the item \"%s\" is being used by something else."),

	/* cant_delete_folder */
	N_("Sorry, but the folder \"%s\" cannot be deleted."),

	/* cant_delete_used_folder */
	N_("Sorry, but the folder \"%s\" is being used by something else."),

	/* delete_marked_items_title */
	N_("Delete marked items"),

	/* delete_marked_items */
	N_("Delete marked items?"),

	/* delete_folder_title */
	N_("Delete folder"),

	/* delete_folder */
	N_("Delete the folder \"%s\" and its content?"),

	/* delete_item_title */
	N_("Delete item"),

	/* delete_item */
	N_("Delete \"%s\"?\n\n%s"),

	/* clear_all_items_title */
	N_("Clear all items"),

	/* clear_all_items */
	N_("Do you really want to remove all items?"),
};

#define listbox_message(msg) \
	ops->messages && ops->messages->msg \
	 ? ops->messages->msg \
	 : default_listbox_ops_messages.msg

static void
print_delete_error(struct listbox_item *item, struct terminal *term,
		   const struct listbox_ops *ops, enum delete_error err)
{
	struct string msg;
	unsigned char *errmsg;
	unsigned char *text;

	switch (err) {
	case DELETE_IMPOSSIBLE:
		if (item->type == BI_FOLDER) {
			errmsg = listbox_message(cant_delete_folder);
		} else {
			errmsg = listbox_message(cant_delete_item);
		}
		break;

	case DELETE_LOCKED:
		if (item->type == BI_FOLDER) {
			errmsg = listbox_message(cant_delete_used_folder);
		} else {
			errmsg = listbox_message(cant_delete_used_item);
		}
		break;

	default:
		INTERNAL("Bad delete error code (%d)!", err);
		return;
	}

	text = ops->get_text(item, term);

	if (!text || !init_string(&msg)) {
		mem_free_if(text);
		return;
	}

	add_format_to_string(&msg, _(errmsg, term), text);
	mem_free(text);

	if (item->type == BI_LEAF) {
		unsigned char *info = ops->get_info(item, term);

		if (info) {
			add_format_to_string(&msg, "\n\n%s", info);
			mem_free(info);
		}
	}

	info_box(term, MSGBOX_FREE_TEXT, N_("Delete error"), ALIGN_LEFT,
		 msg.source);
}

static void
do_delete_item(struct listbox_item *item, struct listbox_context *info,
	       int last)
{
	const struct listbox_ops *ops = info->box->ops;

	assert(item);

	if (!ops->can_delete(item)) {
		print_delete_error(item, info->term, ops, DELETE_IMPOSSIBLE);
		return;
	}

	if (ops->is_used(item)) {
		print_delete_error(item, info->term, ops, DELETE_LOCKED);
		return;
	}

	ops->delete(item, last);
}

static int
delete_marked(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *context = data_;

	if (item->marked && !context->box->ops->is_used(item)) {
		/* Save the first marked so it can be deleted last */
		if (!context->item) {
			context->item = item;
		} else {
			do_delete_item(item, context, 0);
		}

		return 1;
	}

	return 0;
}

static void
push_ok_delete_button(void *context_)
{
	struct listbox_context *context = context_;
	struct listbox_item *root;
	int last = 0;

	if (context->item) {
		context->box->ops->unlock(context->item);
	} else {
		traverse_listbox_items_list(context->box->items->next,
					    context->box, 0, 0,
					    delete_marked, context);
		if (!context->item) return;
	}

	root = context->box->ops->get_root(context->item);
	if (root) {
		last = context->item == root->child.prev;
	}

	/* Delete the last one (traversal should save one to delete) */
	do_delete_item(context->item, context, 1);

	/* If removing the last item in a folder move focus to previous item in
	 * the folder or the root. */
	if (last)
		listbox_sel_move(context->widget_data, -1);
}

static widget_handler_status_T
query_delete_selected_item(void *context_)
{
	/* [gettext_accelerator_context(query_delete_selected_item)] */
	struct listbox_context *context, *oldcontext = context_;
	struct terminal *term = oldcontext->term;
	struct listbox_data *box = oldcontext->box;
	const struct listbox_ops *ops = box->ops;
	struct listbox_item *item = box->sel;
	unsigned char *text;
	enum delete_error delete;

	assert(item);

	delete = ops->can_delete(item) ? DELETE_LOCKED : DELETE_IMPOSSIBLE;

	if (delete == DELETE_IMPOSSIBLE || ops->is_used(item)) {
		print_delete_error(item, term, ops, delete);
		return EVENT_PROCESSED;
	}

	context = init_listbox_context(box, term, item, NULL);
	if (!context) return EVENT_PROCESSED;

	context->widget_data = oldcontext->widget_data;

	text = ops->get_text(item, term);
	if (!text) {
		mem_free(context);
		return EVENT_PROCESSED;
	}

	if (item->type == BI_FOLDER) {
		ops->lock(item);
		msg_box(term, getml(context, (void *) NULL), MSGBOX_FREE_TEXT,
			listbox_message(delete_folder_title), ALIGN_CENTER,
			msg_text(term, listbox_message(delete_folder), text),
			context, 2,
			MSG_BOX_BUTTON(N_("~Yes"), push_ok_delete_button, B_ENTER),
			MSG_BOX_BUTTON(N_("~No"), done_listbox_context, B_ESC));
	} else {
		unsigned char *msg = ops->get_info(item, term);

		ops->lock(item);

		msg_box(term, getml(context, (void *) NULL), MSGBOX_FREE_TEXT,
			listbox_message(delete_item_title), ALIGN_LEFT,
			msg_text(term, listbox_message(delete_item),
			         text, empty_string_or_(msg)),
			context, 2,
			MSG_BOX_BUTTON(N_("~Yes"), push_ok_delete_button, B_ENTER),
			MSG_BOX_BUTTON(N_("~No"), done_listbox_context, B_ESC));
		mem_free_if(msg);
	}
	mem_free(text);

	return EVENT_PROCESSED;
}

static void
dont_delete_marked_items(void *const context_)
{
	query_delete_selected_item(context_);
}

widget_handler_status_T
push_hierbox_delete_button(struct dialog_data *dlg_data,
			   struct widget_data *button)
{
	/* [gettext_accelerator_context(push_hierbox_delete_button)] */
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	const struct listbox_ops *ops = box->ops;
	struct listbox_item *item = box->sel;
	struct listbox_context *context;

	if (!item) return EVENT_PROCESSED;

	assert(ops && ops->can_delete && ops->delete);

	context = init_listbox_context(box, term, item, scan_for_marks);
	if (!context) return EVENT_PROCESSED;

	context->widget_data = dlg_data->widgets_data;

	if (context->item) {
		widget_handler_status_T status;

		status = query_delete_selected_item(context);
		mem_free(context);

		return status;
	}

	msg_box(term, getml(context, (void *) NULL), 0,
		listbox_message(delete_marked_items_title), ALIGN_CENTER,
		listbox_message(delete_marked_items),
		context, 2,
		MSG_BOX_BUTTON(N_("~Yes"), push_ok_delete_button, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), dont_delete_marked_items, B_ESC));

	return EVENT_PROCESSED;
}



/* Clear action */

static int
delete_unused(struct listbox_item *item, void *data_, int *offset)
{
	struct listbox_context *context = data_;

	if (context->box->ops->is_used(item)) return 0;

	do_delete_item(item, context, 0);
	return 1;
}

static void
do_clear_browser(void *context_)
{
	struct listbox_context *context = context_;

	traverse_listbox_items_list(context->box->items->next,
				    context->box, 0, 0,
				    delete_unused, context);
}

widget_handler_status_T
push_hierbox_clear_button(struct dialog_data *dlg_data,
			  struct widget_data *button)
{
	/* [gettext_accelerator_context(push_hierbox_clear_button)] */
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	const struct listbox_ops *ops = box->ops;
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;

	if (!box->sel) return EVENT_PROCESSED;

	assert(ops);

	context = init_listbox_context(box, term, NULL, scan_for_used);
	if (!context) return EVENT_PROCESSED;

	if (context->item) {
		/* FIXME: If the clear button should be used for browsers where
		 * not all items can be deleted scan_for_used() should also can
		 * for undeletable and we should be able to pass either delete
		 * error types. */
		print_delete_error(context->item, term, ops, DELETE_LOCKED);
		mem_free(context);
		return EVENT_PROCESSED;
	}

	msg_box(term, getml(context, (void *) NULL), 0,
		listbox_message(clear_all_items_title), ALIGN_CENTER,
		listbox_message(clear_all_items),
		context, 2,
		MSG_BOX_BUTTON(N_("~Yes"), do_clear_browser, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), NULL, B_ESC));

	return EVENT_PROCESSED;
}

#undef listbox_message


/* Search action */

static int
scan_for_matches(struct listbox_item *item, void *info_, int *offset)
{
	struct listbox_context *context = info_;
	unsigned char *text = (unsigned char *) context->widget_data;

	if (!*text) {
		item->visible = 1;
		return 0;
	}

	switch (context->box->ops->match(item, context->term, text)) {
	case LISTBOX_MATCH_OK:
		/* Mark that we have a match by setting the item to non-NULL */
		context->item = item;
		item->visible = 1;
		break;

	case LISTBOX_MATCH_NO:
		item->visible = 0;
		break;

	case LISTBOX_MATCH_IMPOSSIBLE:
		break;
	}

	return 0;
}

static int
mark_visible(struct listbox_item *item, void *xxx, int *offset)
{
	item->visible = 1;
	return 0;
}


static void
search_hierbox_browser(void *data, unsigned char *text)
{
	struct dialog_data *dlg_data = data;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct listbox_context *context;

	context = init_listbox_context(box, term, NULL, NULL);
	if (!context) return;

	/* Eeew :/ */
	context->widget_data = (void *) text;

	traverse_listbox_items_list(box->items->next, box, 0, 0,
				    scan_for_matches, context);

	if (!context->item && *text) {
		switch (get_opt_int("document.browse.search.show_not_found")) {
		case 2:
			info_box(term, MSGBOX_FREE_TEXT,
				 N_("Search"), ALIGN_CENTER,
				 msg_text(term,
					  N_("Search string '%s' not found"),
					  text));
			break;

		case 1:
			beep_terminal(term);

		default:
			break;
		}

		traverse_listbox_items_list(box->items->next, box, 0, 0,
					    mark_visible, NULL);
	}

	mem_free(context);
}

widget_handler_status_T
push_hierbox_search_button(struct dialog_data *dlg_data,
			   struct widget_data *button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);

	if (!box->sel) return EVENT_PROCESSED;

	assert(box->ops->match);

	input_dialog(term, NULL, N_("Search"), N_("Name"),
		     dlg_data, NULL,
		     MAX_STR_LEN, "", 0, 0, NULL,
		     search_hierbox_browser, NULL);

	return EVENT_PROCESSED;
}
