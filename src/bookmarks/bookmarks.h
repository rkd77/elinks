/* $Id: bookmarks.h,v 1.36 2004/07/15 00:45:33 jonas Exp $ */

#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

#include "modules/module.h"
#include "util/lists.h"
#include "util/object.h"

struct listbox_item;
struct terminal;

/* Bookmark record structure */

struct bookmark {
	LIST_HEAD(struct bookmark);

	struct bookmark *root;

	/* This is indeed maintained by bookmarks.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;

	unsigned char *title;   /* title of bookmark */
	unsigned char *url;     /* Location of bookmarked item */
	struct object object;	/* No direct access, use provided macros for that. */

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
struct bookmark *get_bookmark(unsigned char *url);
void bookmark_terminal_tabs(struct terminal *term, unsigned char *foldername);
void bookmark_auto_save_tabs(struct terminal *term);
int update_bookmark(struct bookmark *, unsigned char *, unsigned char *);
void open_bookmark_folder(struct session *ses, unsigned char *foldername);

#endif
