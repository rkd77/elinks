/* Internal bookmarks support */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bookmarks/backend/common.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "config/home.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "util/conv.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"

/* The list of bookmarks */
INIT_LIST_OF(struct bookmark, bookmarks);

/* Set to 1, if bookmarks have changed. */
static int bookmarks_dirty = 0;

static struct hash *bookmark_cache = NULL;

static struct bookmark *bm_snapshot_last_folder;


/* Life functions */

static struct option_info bookmark_options_info[] = {
	INIT_OPT_TREE("", N_("Bookmarks"),
		"bookmarks", 0,
		N_("Bookmark options.")),

#ifdef CONFIG_XBEL_BOOKMARKS
	INIT_OPT_INT("bookmarks", N_("File format"),
		"file_format", 0, 0, 1, 0,
		N_("File format for bookmarks (affects both reading and "
		"saving):\n"
		"0 is the default native ELinks format\n"
		"1 is XBEL universal XML bookmarks format")),
#else
	INIT_OPT_INT("bookmarks", N_("File format"),
		"file_format", 0, 0, 1, 0,
		N_("File format for bookmarks (affects both reading and "
		"saving):\n"
		"0 is the default native ELinks format\n"
		"1 is XBEL universal XML bookmarks format  (DISABLED)")),
#endif

	INIT_OPT_BOOL("bookmarks", N_("Save folder state"),
		"folder_state", 0, 1,
		N_("When saving bookmarks also store whether folders are "
		"expanded or not, so the look of the bookmark dialog is "
		"kept across ELinks sessions. If disabled all folders will "
		"appear unexpanded next time ELinks is run.")),

	INIT_OPT_BOOL("ui.sessions", N_("Periodic snapshotting"),
		"snapshot", 0, 0,
		N_("Automatically save a snapshot of all tabs periodically. "
		"This will periodically bookmark the tabs of each terminal "
		"in a separate folder for recovery after a crash.\n"
		"\n"
		"This feature requires bookmark support.")),

	NULL_OPTION_INFO
};

static enum evhook_status bookmark_change_hook(va_list ap, void *data);
static enum evhook_status bookmark_write_hook(va_list ap, void *data);

struct event_hook_info bookmark_hooks[] = {
	{ "bookmark-delete", 0, bookmark_change_hook, NULL },
	{ "bookmark-move",   0, bookmark_change_hook, NULL },
	{ "bookmark-update", 0, bookmark_change_hook, NULL },
	{ "periodic-saving", 0, bookmark_write_hook,  NULL },

	NULL_EVENT_HOOK_INFO,
};

static enum evhook_status
bookmark_change_hook(va_list ap, void *data)
{
	struct bookmark *bookmark = va_arg(ap, struct bookmark *);

	if (bookmark == bm_snapshot_last_folder)
		bm_snapshot_last_folder = NULL;

	return EVENT_HOOK_STATUS_NEXT;
}

static void bookmark_snapshot();

static enum evhook_status
bookmark_write_hook(va_list ap, void *data)
{
	if (get_opt_bool("ui.sessions.snapshot")
	    && !get_cmd_opt_bool("anonymous"))
		bookmark_snapshot();

	write_bookmarks();

	return EVENT_HOOK_STATUS_NEXT;
}


static int
change_hook_folder_state(struct session *ses, struct option *current,
			 struct option *changed)
{
	if (!changed->value.number) {
		/* We are to collapse all folders on exit; mark bookmarks dirty
		 * to ensure that this will happen. */
		bookmarks_set_dirty();
	}

	return 0;
}

static void
init_bookmarks(struct module *module)
{
	static const struct change_hook_info bookmarks_change_hooks[] = {
		{ "bookmarks.folder_state", change_hook_folder_state },
		{ NULL,			    NULL },
	};

	register_change_hooks(bookmarks_change_hooks);

	read_bookmarks();
}

/* Clears the bookmark list */
static void
free_bookmarks(LIST_OF(struct bookmark) *bookmarks_list,
	       LIST_OF(struct listbox_item) *box_items)
{
	struct bookmark *bm;

	foreach (bm, *bookmarks_list) {
		if (!list_empty(bm->child))
			free_bookmarks(&bm->child, &bm->box_item->child);
		mem_free(bm->title);
		mem_free(bm->url);
	}

	free_list(*box_items);
	free_list(*bookmarks_list);
	if (bookmark_cache) free_hash(&bookmark_cache);
}

/* Does final cleanup and saving of bookmarks */
static void
done_bookmarks(struct module *module)
{
	/* This is a clean shutdown, so delete the last snapshot. */
	if (bm_snapshot_last_folder) delete_bookmark(bm_snapshot_last_folder);
	bm_snapshot_last_folder = NULL;

	write_bookmarks();
	free_bookmarks(&bookmarks, &bookmark_browser.root.child);
	free_last_searched_bookmark();
}

struct module bookmarks_module = struct_module(
	/* name: */		N_("Bookmarks"),
	/* options: */		bookmark_options_info,
	/* hooks: */		bookmark_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_bookmarks,
	/* done: */		done_bookmarks
);



/* Read/write wrappers */

/* Loads the bookmarks from file */
void
read_bookmarks(void)
{
	bookmarks_read();
}

void
write_bookmarks(void)
{
	if (get_cmd_opt_bool("anonymous")) {
		bookmarks_unset_dirty();
		return;
	}
	bookmarks_write(&bookmarks);
}




/* Bookmarks manipulation */

void
bookmarks_set_dirty(void)
{
	bookmarks_dirty = 1;
}

void
bookmarks_unset_dirty(void)
{
	bookmarks_dirty = 0;
}

int
bookmarks_are_dirty(void)
{
	return (bookmarks_dirty == 1);
}

#define check_bookmark_cache(url) (bookmark_cache && (url) && *(url))

static void
done_bookmark(struct bookmark *bm)
{
	done_listbox_item(&bookmark_browser, bm->box_item);

	mem_free(bm->title);
	mem_free(bm->url);
	mem_free(bm);
}

void
delete_bookmark(struct bookmark *bm)
{
	static int delete_bookmark_event_id = EVENT_NONE;

	while (!list_empty(bm->child)) {
		delete_bookmark(bm->child.next);
	}

	if (check_bookmark_cache(bm->url)) {
		struct hash_item *item;

		item = get_hash_item(bookmark_cache, bm->url, strlen(bm->url));
		if (item) del_hash_item(bookmark_cache, item);
	}

	set_event_id(delete_bookmark_event_id, "bookmark-delete");
	trigger_event(delete_bookmark_event_id, bm);

	del_from_list(bm);
	bookmarks_set_dirty();

	done_bookmark(bm);
}

/** Deletes any bookmarks with no URLs (i.e., folders) and of which
 * the title matches the given argument.
 *
 * @param foldername
 *  The title of the folder, in UTF-8.  */
static void
delete_folder_by_name(const unsigned char *foldername)
{
	struct bookmark *bookmark, *next;

	foreachsafe (bookmark, next, bookmarks) {
		if ((bookmark->url && *bookmark->url)
		    || strcmp(bookmark->title, foldername))
			continue;

		delete_bookmark(bookmark);
	}
}

/** Allocate and initialize a bookmark in the given folder.  This
 * however does not set bookmark.box_item; use add_bookmark() for
 * that.
 *
 * @param root
 *   The folder in which to add the bookmark, or NULL to add it at
 *   top level.
 * @param title
 *   Title of the bookmark.  Must be in UTF-8 and not NULL.
 *   "-" means add a separator.
 * @param url
 *   URL to which the bookmark will point.  Must be in UTF-8.
 *   NULL or "" means add a bookmark folder.
 *
 * @return the new bookmark, or NULL on error.  */
static struct bookmark *
init_bookmark(struct bookmark *root, unsigned char *title, unsigned char *url)
{
	struct bookmark *bm;

	bm = mem_calloc(1, sizeof(*bm));
	if (!bm) return NULL;

	bm->title = stracpy(title);
	if (!bm->title) {
		mem_free(bm);
		return NULL;
	}
	sanitize_title(bm->title);

	bm->url = stracpy(empty_string_or_(url));
	if (!bm->url) {
		mem_free(bm->title);
		mem_free(bm);
		return NULL;
	}
	sanitize_url(bm->url);

	bm->root = root;
	init_list(bm->child);

	object_nolock(bm, "bookmark");

	return bm;
}

static void
add_bookmark_item_to_bookmarks(struct bookmark *bm, struct bookmark *root, int place)
{
	/* Actually add it */
	if (place) {
		if (root)
			add_to_list_end(root->child, bm);
		else
			add_to_list_end(bookmarks, bm);
	} else {
		if (root)
			add_to_list(root->child, bm);
		else
			add_to_list(bookmarks, bm);
	}
	bookmarks_set_dirty();

	/* Hash creation if needed. */
	if (!bookmark_cache)
		bookmark_cache = init_hash8();

	/* Create a new entry. */
	if (check_bookmark_cache(bm->url))
		add_hash_item(bookmark_cache, bm->url, strlen(bm->url), bm);
}

/** Add a bookmark to the bookmark list.
 *
 * @param root
 *   The folder in which to add the bookmark, or NULL to add it at
 *   top level.
 * @param place
 *   0 means add to the top.  1 means add to the bottom.
 * @param title
 *   Title of the bookmark.  Must be in UTF-8 and not NULL.
 *   "-" means add a separator.
 * @param url
 *   URL to which the bookmark will point.  Must be in UTF-8.
 *   NULL or "" means add a bookmark folder.
 *
 * @return the new bookmark, or NULL on error.
 *
 * @see add_bookmark_cp() */
struct bookmark *
add_bookmark(struct bookmark *root, int place, unsigned char *title,
	     unsigned char *url)
{
	enum listbox_item_type type;
	struct bookmark *bm;

	bm = init_bookmark(root, title, url);
	if (!bm) return NULL;

	if (url && *url) {
		type = BI_LEAF;
	} else if (title && title[0] == '-' && title[1] == '\0') {
		type = BI_SEPARATOR;
	} else {
		type = BI_FOLDER;
	}

	bm->box_item = add_listbox_item(&bookmark_browser,
					root ? root->box_item : NULL,
					type,
					(void *) bm,
					place ? -1 : 1);

	if (!bm->box_item) {
		mem_free(bm->url);
		mem_free(bm->title);
		mem_free(bm);
		return NULL;
	}

	add_bookmark_item_to_bookmarks(bm, root, place);

	return bm;
}

/** Add a bookmark to the bookmark list.
 *
 * @param root
 *   The folder in which to add the bookmark, or NULL to add it at
 *   top level.
 * @param place
 *   0 means add to the top.  1 means add to the bottom.
 * @param codepage
 *   Codepage of @a title and @a url.
 * @param title
 *   Title of the bookmark.  Must not be NULL.
 *   "-" means add a separator.
 * @param url
 *   URL to which the bookmark will point.
 *   NULL or "" means add a bookmark folder.
 *
 * @return the new bookmark.
 *
 * @see add_bookmark() */
struct bookmark *
add_bookmark_cp(struct bookmark *root, int place, int codepage,
		unsigned char *title, unsigned char *url)
{
	const int utf8_cp = get_cp_index("UTF-8");
	struct conv_table *table;
	unsigned char *utf8_title = NULL;
	unsigned char *utf8_url = NULL;
	struct bookmark *bookmark = NULL;

	if (!url)
		url = "";

	table = get_translation_table(codepage, utf8_cp);
	if (!table)
		return NULL;

	utf8_title = convert_string(table, title, strlen(title),
				    utf8_cp, CSM_NONE,
				    NULL, NULL, NULL);
	utf8_url = convert_string(table, url, strlen(url),
				  utf8_cp, CSM_NONE,
				  NULL, NULL, NULL);
	if (utf8_title && utf8_url)
		bookmark = add_bookmark(root, place,
					utf8_title, utf8_url);
	mem_free_if(utf8_title);
	mem_free_if(utf8_url);
	return bookmark;
}

/* Updates an existing bookmark.
 *
 * If there's any problem, return 0. Otherwise, return 1.
 *
 * If any of the fields are NULL, the value is left unchanged. */
int
update_bookmark(struct bookmark *bm, int codepage,
		unsigned char *title, unsigned char *url)
{
	static int update_bookmark_event_id = EVENT_NONE;
	const int utf8_cp = get_cp_index("UTF-8");
	struct conv_table *table;
	unsigned char *title2 = NULL;
	unsigned char *url2 = NULL;

	table = get_translation_table(codepage, utf8_cp);
	if (!table)
		return 0;

	if (url) {
		url2 = convert_string(table, url, strlen(url),
				      utf8_cp, CSM_NONE,
				      NULL, NULL, NULL);
		if (!url2) return 0;
		sanitize_url(url2);
	}

	if (title) {
		title2 = convert_string(table, title, strlen(title),
					utf8_cp, CSM_NONE,
					NULL, NULL, NULL);
		if (!title2) {
			mem_free_if(url2);
			return 0;
		}
		sanitize_title(title2);
	}

	set_event_id(update_bookmark_event_id, "bookmark-update");
	trigger_event(update_bookmark_event_id, bm, title2, url2);

	if (title2) {
		mem_free_set(&bm->title, title2);
	}

	if (url2) {
		if (check_bookmark_cache(bm->url)) {
			struct hash_item *item;
			int len = strlen(bm->url);

			item = get_hash_item(bookmark_cache, bm->url, len);
			if (item) del_hash_item(bookmark_cache, item);
		}

		if (check_bookmark_cache(url2)) {
			add_hash_item(bookmark_cache, url2, strlen(url2), bm);
		}

		mem_free_set(&bm->url, url2);
	}

	bookmarks_set_dirty();

	return 1;
}

/** Search for a bookmark with the given title.  The search does not
 * recurse into subfolders.
 *
 * @param folder
 *   Search in this folder.  NULL means search in the root.
 *
 * @param title
 *   Search for this title.  Must be in UTF-8 and not NULL.
 *
 * @return The bookmark, or NULL if not found.  */
struct bookmark *
get_bookmark_by_name(struct bookmark *folder, unsigned char *title)
{
	struct bookmark *bookmark;
	LIST_OF(struct bookmark) *lh;

	lh = folder ? &folder->child : &bookmarks;

	foreach (bookmark, *lh)
		if (!strcmp(bookmark->title, title)) return bookmark;

	return NULL;
}

/* Search bookmark cache for item matching url. */
struct bookmark *
get_bookmark(unsigned char *url)
{
	struct hash_item *item;

	/** @todo Bug 1066: URLs in bookmark_cache should be UTF-8 */
	if (!check_bookmark_cache(url))
		return NULL;

	/* Search for cached entry. */

	item = get_hash_item(bookmark_cache, url, strlen(url));

	return item ? item->value : NULL;
}

static void
bookmark_terminal(struct terminal *term, struct bookmark *folder)
{
	unsigned char title[MAX_STR_LEN], url[MAX_STR_LEN];
	struct window *tab;
	int term_cp = get_terminal_codepage(term);

	foreachback_tab (tab, term->windows) {
		struct session *ses = tab->data;

		if (!get_current_url(ses, url, MAX_STR_LEN))
			continue;

		if (!get_current_title(ses, title, MAX_STR_LEN))
			continue;

		add_bookmark_cp(folder, 1, term_cp, title, url);
	}
}

/** Create a bookmark for each document on the specified terminal,
 * and a folder to contain those bookmarks.
 *
 * @param term
 *   The terminal whose open documents should be bookmarked.
 *
 * @param foldername
 *   The name of the new bookmark folder, in UTF-8.  */
void
bookmark_terminal_tabs(struct terminal *term, unsigned char *foldername)
{
	struct bookmark *folder = add_bookmark(NULL, 1, foldername, NULL);

	if (!folder) return;

	bookmark_terminal(term, folder);
}

static void
bookmark_all_terminals(struct bookmark *folder)
{
	unsigned int n = 0;
	struct terminal *term;

	if (get_cmd_opt_bool("anonymous"))
		return;

	if (list_is_singleton(terminals)) {
		bookmark_terminal(terminals.next, folder);
		return;
	}

	foreach (term, terminals) {
		unsigned char subfoldername[4];
		struct bookmark *subfolder;

		if (ulongcat(subfoldername, NULL, n, sizeof(subfoldername), 0)
		     >= sizeof(subfoldername))
			return;

		++n;

		/* Because subfoldername[] contains only digits,
		 * it is OK as UTF-8.  */
		subfolder = add_bookmark(folder, 1, subfoldername, NULL);
		if (!subfolder) return;

		bookmark_terminal(term, subfolder);
	}
}


unsigned char *
get_auto_save_bookmark_foldername_utf8(void)
{
	unsigned char *foldername;
	int from_cp, to_cp;
	struct conv_table *convert_table;

	foldername = get_opt_str("ui.sessions.auto_save_foldername");
	if (!*foldername) return NULL;

	/* The charset of the string returned by get_opt_str()
	 * seems to be documented nowhere.  Let's assume it is
	 * the system charset.  */
	from_cp = get_cp_index("System");
	to_cp = get_cp_index("UTF-8");
	convert_table = get_translation_table(from_cp, to_cp);
	if (!convert_table) return NULL;

	return convert_string(convert_table,
			      foldername, strlen(foldername),
			      to_cp, CSM_NONE,
			      NULL, NULL, NULL);
}

void
bookmark_auto_save_tabs(struct terminal *term)
{
	unsigned char *foldername; /* UTF-8 */

	if (get_cmd_opt_bool("anonymous")
	    || !get_opt_bool("ui.sessions.auto_save"))
		return;

	foldername = get_auto_save_bookmark_foldername_utf8();
	if (!foldername) return;

	/* Ensure uniqueness of the auto save folder, so it is possible to
	 * restore the (correct) session when starting up. */
	delete_folder_by_name(foldername);

	bookmark_terminal_tabs(term, foldername);
	mem_free(foldername);
}

static void
bookmark_snapshot(void)
{
	struct string folderstring;
	struct bookmark *folder;

	if (!init_string(&folderstring)) return;

	add_to_string(&folderstring, "Session snapshot");

#ifdef HAVE_STRFTIME
	add_to_string(&folderstring, " - ");
	add_date_to_string(&folderstring, get_opt_str("ui.date_format"), NULL);
#endif

	/* folderstring must be in the system codepage because
	 * add_date_to_string() uses strftime().  */
	folder = add_bookmark_cp(NULL, 1, get_cp_index("System"),
				 folderstring.source, NULL);
	done_string(&folderstring);
	if (!folder) return;

	bookmark_all_terminals(folder);

	if (bm_snapshot_last_folder) delete_bookmark(bm_snapshot_last_folder);
	bm_snapshot_last_folder = folder;
}


/** Open all bookmarks from the named folder.
 *
 * @param ses
 *   The session in which to open the first bookmark.  The other
 *   bookmarks of the folder open in new tabs on the same terminal.
 *
 * @param foldername
 *   The name of the bookmark folder, in UTF-8.  */
void
open_bookmark_folder(struct session *ses, unsigned char *foldername)
{
	struct bookmark *bookmark;
	struct bookmark *folder = NULL;
	struct bookmark *current = NULL;

	assert(foldername && ses);
	if_assert_failed return;

	foreach (bookmark, bookmarks) {
		if (bookmark->box_item->type != BI_FOLDER)
			continue;
		if (strcmp(bookmark->title, foldername))
			continue;
		folder = bookmark;
		break;
	}

	if (!folder) return;

	foreach (bookmark, folder->child) {
		struct uri *uri;

		if (bookmark->box_item->type == BI_FOLDER
		    || bookmark->box_item->type == BI_SEPARATOR
		    || !*bookmark->url)
			continue;

		/** @todo Bug 1066: Tell the URI layer that
		 * bookmark->url is UTF-8.  */
		uri = get_translated_uri(bookmark->url, NULL);
		if (!uri) continue;

		/* Save the first bookmark for the current tab */
		if (!current) {
			current = bookmark;
			goto_uri(ses, uri);
		} else {
			open_uri_in_new_tab(ses, uri, 1, 0);
		}

		done_uri(uri);
	}
}
