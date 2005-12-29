#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

#include "main/module.h"
#include "main/object.h"
#include "util/lists.h"

struct listbox_item;
struct terminal;

/* Bookmark record structure */

struct bookmark {
	OBJECT_HEAD(struct bookmark);

	struct bookmark *root;

	struct listbox_item *box_item;

	unsigned char *title;   /* title of bookmark */
	unsigned char *url;     /* Location of bookmarked item */

	struct list_head child;
};

/* Bookmark lists */

extern struct list_head bookmarks; /* struct bookmark */

/* The bookmarks module */

extern struct module bookmarks_module;

/* Read/write bookmarks functions */

void read_bookmarks(void);
void write_bookmarks(void);

/* Bookmarks manipulation */
void bookmarks_set_dirty(void);
void bookmarks_unset_dirty(void);
int bookmarks_are_dirty(void);

void delete_bookmark(struct bookmark *);
struct bookmark *add_bookmark(struct bookmark *, int, unsigned char *, unsigned char *);
struct bookmark *get_bookmark_by_name(struct bookmark *folder,
                                      unsigned char *title);
struct bookmark *get_bookmark(unsigned char *url);
void bookmark_terminal_tabs(struct terminal *term, unsigned char *foldername);
void bookmark_auto_save_tabs(struct terminal *term);
int update_bookmark(struct bookmark *, unsigned char *, unsigned char *);
void open_bookmark_folder(struct session *ses, unsigned char *foldername);

#endif
