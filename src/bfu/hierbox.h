#ifndef EL__BFU_HIERBOX_H
#define EL__BFU_HIERBOX_H

#include "bfu/common.h"
#include "bfu/listbox.h"
#include "util/lists.h"

struct session;

/** BFU hierbox browser button */
struct hierbox_browser_button {
	/** The button label text
	 * It is automatically localized. */
	unsigned char *label;

	/** The button handler
	 * The handler gets called when the button is activated */
	widget_handler_T *handler;

	/** Allow this button in anonymous mode
	 * Should the button be displayed in anonymous mode or not? This
	 * can be used to disable actions that are not safe in anonymous
	 * mode. */
	unsigned int anonymous:1;
};

/** BFU hierbox browser
 *
 * Hierarchic listbox browsers are used for the various (subsystem)
 * managers. They basically consist of some state data maintained
 * throughout the life of the dialog (i.e. the listbox widget),
 * manager specific operations (for modifying the underlying data
 * structures), and some buttons.
 */
struct hierbox_browser {
	/** The title of the browser
	 * Note, it is automatically localized. */
	unsigned char *title;

	/** Callback for (un)expansion of the listboxes
	 * Can be used by subsystems to install a handler to be called
	 * when listboxes are expanded and unexpanded. */
	void (*expansion_callback)(void);

	/** Array of browser buttons
	 *
	 * Each button represents an action for modifying or interacting
	 * with the items in the manager.
	 *
	 * A close button will be installed by default. */
	const struct hierbox_browser_button *buttons;
	/** The number of browser buttons */
	size_t buttons_size;

	/** List of active listbox containers
	 *
	 * Several instantiations of a manager can exist at the same
	 * time, if the user has more than one terminal open. This list
	 * contains all the listbox contains for this particular manager
	 * that are currently open. */
	LIST_OF(struct listbox_data) boxes;
	/** List of active dialogs
	 *
	 * Several instantiations of a manager can exist at the same
	 * time, if the user has more than one terminal open. This list
	 * contains all the manager dialogs for this particular manager
	 * that are currently open. */
	LIST_OF(struct hierbox_dialog_list_item) dialogs;

	/** The root listbox
	 * The ancestor of all listboxes in this listbox browser. */
	struct listbox_item root;

	/** Browser specific listbox operations
	 * The operations for managing the underlying data structures of
	 * the listboxes. */
	const struct listbox_ops *ops;

	/** State saved between invocations
	 * Each time the browser is closed, its current state is saved
	 * in this member so it can be restored when the browser is
	 * opened again. This way the currently selected item can be
	 * preserved across several interactions with the browser. */
	struct listbox_data box_data;
	/** Option for not saving the state
	 * Some browsers with highly dynamic content should not have
	 * their state restored. This member can be used to mark such
	 * browsers. */
	unsigned int do_not_save_state:1;
};

/** Define a hierbox browser
 * This macro takes care of initializing all the fields of a hierbox
 * browser so it is ready to use. */
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

struct hierbox_dialog_list_item {
	LIST_HEAD(struct hierbox_dialog_list_item);

	struct dialog_data *dlg_data;
};

void done_listbox_item(struct hierbox_browser *browser, struct listbox_item *item);
void update_hierbox_browser(struct hierbox_browser *browser);

struct listbox_item *
add_listbox_item(struct hierbox_browser *browser, struct listbox_item *root,
		 enum listbox_item_type type, void *data, int add_position);

#define add_listbox_folder(browser, root,data) \
	add_listbox_item(browser, root, BI_FOLDER, data, 1)

#define add_listbox_leaf(browser, root, data) \
	add_listbox_item(browser, root, BI_LEAF, data, 1)

/** Open a hierbox browser
 * Opens an instantiation of a hierbox browser
 *
 * @param browser	The browser to open.
 * @param ses		The session (and terminal) on which it should appear.
 * @return		A reference to the dialog that was created or NULL.
 */
struct dialog_data *
hierbox_browser(struct hierbox_browser *browser, struct session *ses);

widget_handler_status_T push_hierbox_info_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_goto_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_delete_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_clear_button(struct dialog_data *dlg_data, struct widget_data *button);
widget_handler_status_T push_hierbox_search_button(struct dialog_data *dlg_data, struct widget_data *button);

#endif
