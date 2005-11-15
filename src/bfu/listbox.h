#ifndef EL__BFU_LISTBOX_H
#define EL__BFU_LISTBOX_H

#include "util/align.h"
#include "util/lists.h"

struct dialog;
struct listbox_data;
struct listbox_item;
struct terminal;
struct uri;
struct widget_data;

struct widget_info_listbox {
	int height;
};


void add_dlg_listbox(struct dialog *dlg, int height, void *box_data);

enum listbox_match {
	LISTBOX_MATCH_OK,
	LISTBOX_MATCH_NO,
	LISTBOX_MATCH_IMPOSSIBLE,
};

/* Structure that can be used for storing all relevant info when traversing
 * listboxes. */
struct listbox_context {
	/* The terminal we are running on */
	struct terminal *term;

	/* Used for saving a specific item that should be used later when
	 * traversing listboxes has ended. */
	struct listbox_item *item;

	/* The current listbox widgets data */
	struct listbox_data *box;

	/* The current (hierbox browser) dialog stuff */
	struct dialog_data *dlg_data;
	struct widget_data *widget_data;

	/* Used when distributing? the current selected to another position */
	int dist;

	/* The offset of the current box from the top */
	int offset;
};

struct listbox_ops_messages {
	unsigned char *cant_delete_item;
	unsigned char *cant_delete_used_item;
	unsigned char *cant_delete_folder;
	unsigned char *cant_delete_used_folder;
	unsigned char *delete_marked_items_title;
	unsigned char *delete_marked_items;
	unsigned char *delete_folder_title;
	unsigned char *delete_folder;
	unsigned char *delete_item_title;
	unsigned char *delete_item;
	unsigned char *clear_all_items_title;
	unsigned char *clear_all_items;
};

/* TODO: We can maybe find a better way of figuring out whether a user of a
 * generic button handler has implemented all the required functions. --jonas
 * */
struct listbox_ops {
	/* Some basic util/object.h wrappers */
	void (*lock)(struct listbox_item *);
	void (*unlock)(struct listbox_item *);
	int (*is_used)(struct listbox_item *);

	unsigned char *(*get_text)(struct listbox_item *, struct terminal *);
	unsigned char *(*get_info)(struct listbox_item *, struct terminal *);

	struct uri *(*get_uri)(struct listbox_item *);

	struct listbox_item *(*get_root)(struct listbox_item *);

	/* Do a search on the item. */
	enum listbox_match (*match)(struct listbox_item *, struct terminal *,
				    unsigned char *text);

	/* Before calling delete() thou shall call can_delete(). */
	int (*can_delete)(struct listbox_item *);

	/* Delete the listbox item object, its data and all nested listboxes.
	 * @last is non zero when either deleting only one item or when
	 * deleting the last item. */
	void (*delete)(struct listbox_item *, int last);

	/* If defined it means that the it will control all drawing of the
	 * listbox item text and what might else be possible on the screen on
	 * line @y from @x and @width chars onwards. */
	void (*draw)(struct listbox_item *, struct listbox_context *,
		     int x, int y, int width);

	struct listbox_ops_messages *messages;
};

/* Stores display information about a box. Kept in cdata. */
struct listbox_data {
	LIST_HEAD(struct listbox_data);

	struct listbox_ops *ops; /* Backend-provided operations */
	struct listbox_item *sel; /* Item currently selected */
	struct listbox_item *top; /* Item which is on the top line of the box */

	int sel_offset; /* Offset of selected item against the box top */
	struct list_head *items; /* The list being displayed */
};

enum listbox_item_type {
	BI_LEAF,
	BI_FOLDER,
	BI_SEPARATOR
};

/* An item in a box */
struct listbox_item {
	LIST_HEAD(struct listbox_item);

	/* The list may be empty for leaf nodes or non-hiearchic listboxes */
	struct list_head child;

	enum listbox_item_type type;
	int depth;

	unsigned int expanded:1; /* Only valid if this is a BI_FOLDER */
	unsigned int visible:1; /* Is this item visible? */
	unsigned int marked:1;

	void *udata;
};

extern struct widget_ops listbox_ops;

void dlg_format_listbox(struct terminal *, struct widget_data *, int, int *, int, int, int *, enum format_align);

struct listbox_item *traverse_listbox_items_list(struct listbox_item *, struct listbox_data *, int, int, int (*)(struct listbox_item *, void *, int *), void *);

void listbox_sel_move(struct widget_data *, int);
void listbox_sel(struct widget_data *widget_data, struct listbox_item *item);
struct listbox_data *get_listbox_widget_data(struct widget_data *widget_data);

#define get_dlg_listbox_data(dlg_data) \
	get_listbox_widget_data(dlg_data->widgets_data)

#endif
