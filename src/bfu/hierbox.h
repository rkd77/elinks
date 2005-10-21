
#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/common.h"
#include "bfu/listbox.h"
#include "util/lists.h"

struct session;

struct hierbox_browser_button {
	unsigned char *label;
	widget_handler_T *handler;

	/* Should the button be displayed in anonymous mode */
	unsigned int anonymous:1;
};

struct hierbox_browser {
	unsigned char *title;
	void (*expansion_callback)(void);
	struct hierbox_browser_button *buttons;
	size_t buttons_size;

	struct list_head boxes;
	struct list_head dialogs;
	struct listbox_item root;
	struct listbox_ops *ops;

	/* For saving state */
	unsigned int do_not_save_state:1;
	struct listbox_data box_data;
};

#define struct_hierbox_browser(name, title, buttons, ops)		\
	struct hierbox_browser name = {					\
		title,							\
		NULL,							\
		buttons,						\
		sizeof_array(buttons),					\
		{ D_LIST_HEAD(name.boxes) },				\
		{ D_LIST_HEAD(name.dialogs) },				\
		{							\
			NULL_LIST_HEAD,					\
			{ D_LIST_HEAD(name.root.child) },		\
			BI_FOLDER,					\
			-1,						\
			1,						\
			0,						\
		},							\
		ops,							\
	}

void done_listbox_item(struct hierbox_browser *browser, struct listbox_item *box_item);
void update_hierbox_browser(struct hierbox_browser *browser);

struct listbox_item *
add_listbox_item(struct hierbox_browser *browser, struct listbox_item *root,
		 enum listbox_item_type type, void *data, int add_position);

#define add_listbox_folder(browser, root,data) \
	add_listbox_item(browser, root, BI_FOLDER, data, 1)

#define add_listbox_leaf(browser, root, data) \
	add_listbox_item(browser, root, BI_LEAF, data, 1)

/* We use hierarchic listbox browsers for the various managers. They consist
 * of a listbox widget and some buttons.
 *
 * @term	The terminal where the browser should appear.
 *
 * @title	The title of the browser. It is automatically localized.
 *
 * @add_size	The size of extra data to be allocated with the dialog.
 *
 * @browser	The browser structure that contains info to setup listbox data
 *		and manage the dialog list to keep instances of the browser in
 *		sync on various terminals.
 *
 * @udata	Is a reference to any data that the dialog could use.
 *
 * @buttons	Denotes the number of buttons given as varadic arguments.
 *		For each button 4 arguments are extracted:
 *			o First the label text. It is automatically localized.
 *			  If NULL, this button is skipped.
 *			o Second a pointer to a widget handler.
 *		XXX: A close button will be installed by default.
 *
 * XXX: Note that the @listbox_data is detached and freed by the dialog handler.
 *	Any other requirements should be handled by installing a specific
 *	dlg->abort handler. */

struct dialog_data *
hierbox_browser(struct hierbox_browser *browser, struct session *ses);

widget_handler_status_T push_hierbox_info_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_goto_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_delete_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_clear_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_search_button(struct dialog_data *dlg_data, struct widget_data *button);

#endif
