#ifndef EL__BOOKMARKS_BOOKMARKS_H
#define EL__BOOKMARKS_BOOKMARKS_H

#include "main/module.h"
#include "main/object.h"
#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

struct listbox_item;
struct terminal;

/* Bookmark record structure */

struct bookmark {
	OBJECT_HEAD(struct bookmark);

	struct bookmark *root;

	struct listbox_item *box_item;

	/** @todo Bug 1066: The bookmark::url string should be in UTF-8 too,
	 * but this has not yet been fully implemented.  */
	char *title;   /* UTF-8 title of bookmark */
	char *url;     /* Location of bookmarked item */

	LIST_OF(struct bookmark) child;
};

/* Bookmark lists */

extern LIST_OF(struct bookmark) bookmarks;

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
struct bookmark *add_bookmark(struct bookmark *, int, const char *, const char *);
struct bookmark *add_bookmark_cp(struct bookmark *, int, int,
				 const char *, const char *);
struct bookmark *get_bookmark_by_name(struct bookmark *folder,
                                      char *title);
struct bookmark *get_bookmark(char *url);
void bookmark_terminal_tabs(struct terminal *term, char *foldername);
char *get_auto_save_bookmark_foldername_utf8(void);
void bookmark_auto_save_tabs(struct terminal *term);
int update_bookmark(struct bookmark *, int,
		    char *, char *);
void open_bookmark_folder(struct session *ses, char *foldername);

#ifdef __cplusplus
}
#endif

#endif
