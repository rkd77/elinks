/* Bookmarks dialogs */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "bfu/listbox.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/time.h"


/* Whether to save bookmarks after each modification of their list
 * (add/modify/delete). */
#define BOOKMARKS_RESAVE	1


static void
lock_bookmark(struct listbox_item *item)
{
	object_lock((struct bookmark *) item->udata);
}

static void
unlock_bookmark(struct listbox_item *item)
{
	object_unlock((struct bookmark *) item->udata);
}

static int
is_bookmark_used(struct listbox_item *item)
{
	return is_object_used((struct bookmark *) item->udata);
}

static unsigned char *
get_bookmark_text(struct listbox_item *item, struct terminal *term)
{
	struct bookmark *bookmark = item->udata;
	int utf8_cp = get_cp_index("UTF-8");
	int term_cp = get_terminal_codepage(term);
	struct conv_table *convert_table;

	convert_table = get_translation_table(utf8_cp, term_cp);
	if (!convert_table) return NULL;

	return convert_string(convert_table,
			      bookmark->title, strlen(bookmark->title),
			      term_cp, CSM_NONE, NULL, NULL, NULL);
}

/** A callback for convert_string().  This ignores errors and can
 * result in truncated strings if out of memory.  Accordingly, the
 * resulting string may be displayed in the UI but should not be saved
 * to a file or given to another program.  */
static void
add_converted_bytes_to_string(void *data, unsigned char *buf, int buflen)
{
	struct string *string = data;

	add_bytes_to_string(string, buf, buflen); /* ignore errors */
}

static unsigned char *
get_bookmark_info(struct listbox_item *item, struct terminal *term)
{
	struct bookmark *bookmark = item->udata;
	int utf8_cp = get_cp_index("UTF-8");
	int term_cp = get_terminal_codepage(term);
	struct conv_table *convert_table;
	struct string info;

	if (item->type == BI_FOLDER) return NULL;
	convert_table = get_translation_table(utf8_cp, term_cp);
	if (!convert_table) return NULL;
	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: ", _("Title", term));
	convert_string(convert_table, bookmark->title, strlen(bookmark->title),
		       term_cp, CSM_NONE, NULL,
		       add_converted_bytes_to_string, &info);
	add_format_to_string(&info, "\n%s: ", _("URL", term));
	convert_string(convert_table, bookmark->url, strlen(bookmark->url),
		       term_cp, CSM_NONE, NULL,
		       add_converted_bytes_to_string, &info);

	return info.source;
}

static struct uri *
get_bookmark_uri(struct listbox_item *item)
{
	struct bookmark *bookmark = item->udata;

	/** @todo Bug 1066: Tell the URI layer that bookmark->url is UTF-8.  */
	return bookmark->url && *bookmark->url
		? get_translated_uri(bookmark->url, NULL) : NULL;
}

static struct listbox_item *
get_bookmark_root(struct listbox_item *item)
{
	struct bookmark *bookmark = item->udata;

	return bookmark->root ? bookmark->root->box_item : NULL;
}

static int
can_delete_bookmark(struct listbox_item *item)
{
	return 1;
}

static void
delete_bookmark_item(struct listbox_item *item, int last)
{
	struct bookmark *bookmark = item->udata;

	assert(!is_object_used(bookmark));

	delete_bookmark(bookmark);

#ifdef BOOKMARKS_RESAVE
	if (last) write_bookmarks();
#endif
}

static struct listbox_ops_messages bookmarks_messages = {
	/* cant_delete_item */
	N_("Sorry, but the bookmark \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but the bookmark \"%s\" is being used by something else."),
	/* cant_delete_folder */
	N_("Sorry, but the folder \"%s\" cannot be deleted."),
	/* cant_delete_used_folder */
	N_("Sorry, but the folder \"%s\" is being used by something else."),
	/* delete_marked_items_title */
	N_("Delete marked bookmarks"),
	/* delete_marked_items */
	N_("Delete marked bookmarks?"),
	/* delete_folder_title */
	N_("Delete folder"),
	/* delete_folder */
	N_("Delete the folder \"%s\" and all bookmarks in it?"),
	/* delete_item_title */
	N_("Delete bookmark"),
	/* delete_item; xgettext:c-format */
	N_("Delete this bookmark?"),
	/* clear_all_items_title */
	N_("Clear all bookmarks"),
	/* clear_all_items_title */
	N_("Do you really want to remove all bookmarks?"),
};

static const struct listbox_ops bookmarks_listbox_ops = {
	lock_bookmark,
	unlock_bookmark,
	is_bookmark_used,
	get_bookmark_text,
	get_bookmark_info,
	get_bookmark_uri,
	get_bookmark_root,
	NULL,
	can_delete_bookmark,
	delete_bookmark_item,
	NULL,
	&bookmarks_messages,
};

/****************************************************************************
  Bookmark manager stuff.
****************************************************************************/

/* Callback for the "add" button in the bookmark manager */
static widget_handler_status_T
push_add_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	launch_bm_add_doc_dialog(dlg_data->win->term, dlg_data,
				 (struct session *) dlg_data->dlg->udata);
	return EVENT_PROCESSED;
}


static
void launch_bm_search_doc_dialog(struct terminal *, struct dialog_data *,
				 struct session *);


/* Callback for the "search" button in the bookmark manager */
static widget_handler_status_T
push_search_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	launch_bm_search_doc_dialog(dlg_data->win->term, dlg_data,
				    (struct session *) dlg_data->dlg->udata);
	return EVENT_PROCESSED;
}


static void
move_bookmark_after_selected(struct bookmark *bookmark, struct bookmark *selected)
{
	if (selected == bookmark->root
	    || !selected
	    || !selected->box_item
	    || !bookmark->box_item)
		return;

	del_from_list(bookmark->box_item);
	del_from_list(bookmark);

	add_at_pos(selected, bookmark);
	add_at_pos(selected->box_item, bookmark->box_item);
}

/** Add a bookmark; if called from the bookmark manager, also move
 * the bookmark to the right place and select it in the manager.
 * And possibly save the bookmarks.
 *
 * @param term
 *   The terminal whose user told ELinks to add the bookmark.
 *   Currently, @a term affects only the charset interpretation
 *   of @a title and @a url.  In the future, this function could
 *   also display error messages in @a term.
 *
 * @param dlg_data
 *   The bookmark manager dialog, or NULL if the bookmark is being
 *   added without involving the bookmark manager.  If @a dlg_data
 *   is not NULL, dlg_data->win->term should be @a term.
 *
 * @param title
 *   The title of the new bookmark, in the encoding of @a term.
 *   Must not be NULL.  "-" means add a separator.
 *
 * @param url
 *   The URL of the new bookmark, in the encoding of @a term.  NULL
 *   or "" means add a bookmark folder, unless @a title is "-".  */
static void
do_add_bookmark(struct terminal *term, struct dialog_data *dlg_data,
		unsigned char *title, unsigned char *url)
{
	int term_cp = get_terminal_codepage(term);
	struct bookmark *bm = NULL;
	struct bookmark *selected = NULL;
	struct listbox_data *box = NULL;

	if (dlg_data) {
		box = get_dlg_listbox_data(dlg_data);

		if (box->sel) {
			selected = box->sel->udata;

			if (box->sel->type == BI_FOLDER && box->sel->expanded) {
				bm = selected;
			} else {
				bm = selected->root;
			}
		}
	}

	bm = add_bookmark_cp(bm, 1, term_cp, title, url);
	if (!bm) return;

	move_bookmark_after_selected(bm, selected);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif

	if (dlg_data) {
		struct widget_data *widget_data = dlg_data->widgets_data;

		/* We touch only the actual bookmark dialog, not all of them;
		 * that's right, right? ;-) --pasky */
		listbox_sel(widget_data, bm->box_item);
	}
}


/**** ADD FOLDER *****************************************************/

/** Add a bookmark folder.  This is called when the user pushes the OK
 * button in the input dialog that asks for the folder name.
 *
 * @param dlg_data
 *   The bookmark manager.  Must not be NULL.
 *
 * @param foldername
 *   The folder name that the user typed in the input dialog.
 *   This is in the charset of the terminal.  */
static void
do_add_folder(struct dialog_data *dlg_data, unsigned char *foldername)
{
	do_add_bookmark(dlg_data->win->term, dlg_data, foldername, NULL);
}

/** Prepare to add a bookmark folder.  This is called when the user
 * pushes the "Add folder" button in the bookmark manager.
 *
 * @param dlg_data
 *   The bookmark manager.  Must not be NULL.
 *
 * @param widget_data
 *   The "Add folder" button.  */
static widget_handler_status_T
push_add_folder_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	input_dialog(dlg_data->win->term, NULL,
		     N_("Add folder"), N_("Folder name"),
		     dlg_data, NULL,
		     MAX_STR_LEN, NULL, 0, 0, NULL,
		     (void (*)(void *, unsigned char *)) do_add_folder,
		     NULL);
	return EVENT_PROCESSED;
}


/**** ADD SEPARATOR **************************************************/

/** Add a bookmark separator.  This is called when the user pushes the
 * "Add separator" button in the bookmark manager.
 *
 * @param dlg_data
 *   The bookmark manager.  Must not be NULL.
 *
 * @param widget_data
 *   The "Add separator" button.  */
static widget_handler_status_T
push_add_separator_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	do_add_bookmark(dlg_data->win->term, dlg_data, "-", "");
	redraw_dialog(dlg_data, 1);
	return EVENT_PROCESSED;
}


/**** EDIT ***********************************************************/

/* Called when an edit is complete. */
static void
bookmark_edit_done(void *data) {
	struct dialog *dlg = data;
	struct bookmark *bm = (struct bookmark *) dlg->udata2;
	struct dialog_data *parent_dlg_data = dlg->udata;
	int term_cp = get_terminal_codepage(parent_dlg_data->win->term);

	update_bookmark(bm, term_cp,
			dlg->widgets[0].data, dlg->widgets[1].data);
	object_unlock(bm);

#ifdef BOOKMARKS_RESAVE
	write_bookmarks();
#endif
}

static void
bookmark_edit_cancel(struct dialog *dlg) {
	struct bookmark *bm = (struct bookmark *) dlg->udata2;

	object_unlock(bm);
}

/* Called when the edit button is pushed */
static widget_handler_status_T
push_edit_button(struct dialog_data *dlg_data, struct widget_data *edit_btn)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);

	/* Follow the bookmark */
	if (box->sel) {
		struct bookmark *bm = (struct bookmark *) box->sel->udata;
		int utf8_cp = get_cp_index("UTF-8");
		int term_cp = get_terminal_codepage(dlg_data->win->term);
		struct conv_table *convert_table;

		convert_table = get_translation_table(utf8_cp, term_cp);
		if (convert_table) {
			unsigned char *title;
			unsigned char *url;

			title = convert_string(convert_table,
					       bm->title, strlen(bm->title),
					       term_cp, CSM_NONE,
					       NULL, NULL, NULL);
			url = convert_string(convert_table,
					     bm->url, strlen(bm->url),
					     term_cp, CSM_NONE,
					     NULL, NULL, NULL);
			if (title && url) {
				object_lock(bm);
				do_edit_dialog(dlg_data->win->term, 1,
					       N_("Edit bookmark"),
					       title, url,
					       (struct session *) dlg_data->dlg->udata,
					       dlg_data,
					       bookmark_edit_done,
					       bookmark_edit_cancel,
					       (void *) bm, EDIT_DLG_ADD);
			}
			mem_free_if(title);
			mem_free_if(url);
		}
	}

	return EVENT_PROCESSED;
}


/**** MOVE ***********************************************************/

static struct bookmark *move_cache_root_avoid;

static void
update_depths(struct listbox_item *parent)
{
	struct listbox_item *item;

	foreach (item, parent->child) {
		item->depth = parent->depth + 1;
		if (item->type == BI_FOLDER)
			update_depths(item);
	}
}

enum move_bookmark_flags {
	MOVE_BOOKMARK_NONE  = 0x00,
	MOVE_BOOKMARK_MOVED = 0x01,
	MOVE_BOOKMARK_CYCLE = 0x02
};

/* Traverse all bookmarks and move all marked items
 * _into_ dest or, if insert_as_child is 0, _after_ dest. */
static enum move_bookmark_flags
do_move_bookmark(struct bookmark *dest, int insert_as_child,
		 LIST_OF(struct bookmark) *src, struct listbox_data *box)
{
	static int move_bookmark_event_id = EVENT_NONE;
	struct bookmark *bm, *next;
	enum move_bookmark_flags result = MOVE_BOOKMARK_NONE;

	set_event_id(move_bookmark_event_id, "bookmark-move");

	foreachsafe (bm, next, *src) {
		if (!bm->box_item->marked) {
			/* Don't move this bookmark itself; but if
			 * it's a folder, then we'll look inside. */
		} else if (bm == dest || bm == move_cache_root_avoid) {
			/* Prevent moving a folder into itself. */
			result |= MOVE_BOOKMARK_CYCLE;
		} else {
			struct hierbox_dialog_list_item *item;

			result |= MOVE_BOOKMARK_MOVED;
			bm->box_item->marked = 0;

			trigger_event(move_bookmark_event_id, bm, dest);

			foreach (item, bookmark_browser.dialogs) {
				struct widget_data *widget_data;
				struct listbox_data *box2;

				widget_data = item->dlg_data->widgets_data;
				box2 = get_listbox_widget_data(widget_data);

				if (box2->top == bm->box_item)
					listbox_sel_move(widget_data, 1);
			}

			del_from_list(bm->box_item);
			del_from_list(bm);
			if (insert_as_child) {
				add_to_list(dest->child, bm);
				add_to_list(dest->box_item->child, bm->box_item);
				bm->root = dest;
				insert_as_child = 0;
			} else {
				add_at_pos(dest, bm);
				add_at_pos(dest->box_item, bm->box_item);
				bm->root = dest->root;
			}

			bm->box_item->depth = bm->root
						? bm->root->box_item->depth + 1
						: 0;

			if (bm->box_item->type == BI_FOLDER)
				update_depths(bm->box_item);

			dest = bm;

			/* We don't want to care about anything marked inside
			 * of the marked folder, let's move it as a whole
			 * directly. I believe that this is more intuitive.
			 * --pasky */
			continue;
		}

		if (bm->box_item->type == BI_FOLDER) {
			result |= do_move_bookmark(dest, insert_as_child,
						   &bm->child, box);
		}
	}

	return result;
}

static widget_handler_status_T
push_move_button(struct dialog_data *dlg_data,
		 struct widget_data *blah)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct bookmark *dest = NULL;
	int insert_as_child = 0;
	enum move_bookmark_flags result;

	if (!box->sel) return EVENT_PROCESSED; /* nowhere to move to */

	dest = box->sel->udata;
	if (box->sel->type == BI_FOLDER && box->sel->expanded) {
		insert_as_child = 1;
	}
	/* Avoid recursion headaches (prevents moving a folder into itself). */
	move_cache_root_avoid = NULL;
	{
		struct bookmark *bm = dest->root;

		while (bm) {
			if (bm->box_item->marked)
				move_cache_root_avoid = bm;
			bm = bm->root;
		}
	}

	result = do_move_bookmark(dest, insert_as_child, &bookmarks, box);
	if (result & MOVE_BOOKMARK_MOVED) {
		bookmarks_set_dirty();

#ifdef BOOKMARKS_RESAVE
		write_bookmarks();
#endif
		update_hierbox_browser(&bookmark_browser);
	}
#ifndef CONFIG_SMALL
	else if (result & MOVE_BOOKMARK_CYCLE) {
		/* If the user also selected other bookmarks, then
		 * they have already been moved, and this box doesn't
		 * appear.  */
		info_box(dlg_data->win->term, 0,
			 N_("Cannot move folder inside itself"), ALIGN_LEFT,
			 N_("You are trying to move the marked folder inside "
			    "itself. To move the folder to a different "
			    "location select the new location before pressing "
			    "the Move button."));
	} else {
		info_box(dlg_data->win->term, 0,
			 N_("Nothing to move"), ALIGN_LEFT,
			 N_("To move bookmarks, first mark all the bookmarks "
			    "(or folders) you want to move.  This can be done "
			    "with the Insert key if you're using the default "
			    "key-bindings.  An asterisk will appear near all "
			    "marked bookmarks.  Now move to where you want to "
			    "have the stuff moved to, and press the \"Move\" "
			    "button."));
	}
#endif	/* ndef CONFIG_SMALL */

	return EVENT_PROCESSED;
}


/**** MANAGEMENT *****************************************************/

static const struct hierbox_browser_button bookmark_buttons[] = {
	/* [gettext_accelerator_context(.bookmark_buttons)] */
	{ N_("~Goto"),		push_hierbox_goto_button,	1 },
	{ N_("~Edit"),		push_edit_button,		0 },
	{ N_("~Delete"),	push_hierbox_delete_button,	0 },
	{ N_("~Add"),		push_add_button,		0 },
	{ N_("Add se~parator"), push_add_separator_button,	0 },
	{ N_("Add ~folder"),	push_add_folder_button,		0 },
	{ N_("~Move"),		push_move_button,		0 },
	{ N_("~Search"),	push_search_button,		1 },
#if 0
	/* This one is too dangerous, so just let user delete
	 * the bookmarks file if needed. --Zas */
	{ N_("Clear"),		push_hierbox_clear_button,	0 },

	/* TODO: Would this be useful? --jonas */
	{ N_("Save"),		push_save_button,		0 },
#endif
};

struct_hierbox_browser(
	bookmark_browser,
	N_("Bookmark manager"),
	bookmark_buttons,
	&bookmarks_listbox_ops
);

/* Builds the "Bookmark manager" dialog */
void
bookmark_manager(struct session *ses)
{
	free_last_searched_bookmark();
	bookmark_browser.expansion_callback = bookmarks_set_dirty;
	hierbox_browser(&bookmark_browser, ses);
}


/****************************************************************************\
  Bookmark search dialog.
\****************************************************************************/


/* Searchs a substring either in title or url fields (ignoring
 * case).  If search_title and search_url are not empty, it selects bookmarks
 * matching the first OR the second.
 *
 * Perhaps another behavior could be to search bookmarks matching both
 * (replacing OR by AND), but it would break a cool feature: when on a page,
 * opening search dialog will have fields corresponding to that page, so
 * pressing ok will find any bookmark with that title or url, permitting a
 * rapid search of an already existing bookmark. --Zas */

struct bookmark_search_ctx {
	unsigned char *url;	/* UTF-8 */
	unsigned char *title;	/* system charset */
	int found;
	int offset;
	int utf8_cp;
	int system_cp;
};

#define NULL_BOOKMARK_SEARCH_CTX {NULL, NULL, 0, 0, -1, -1}

static int
test_search(struct listbox_item *item, void *data_, int *offset)
{
	struct bookmark_search_ctx *ctx = data_;

	if (!ctx->offset) {
		ctx->found = 0; /* ignore possible match on first item */
	} else {
		struct bookmark *bm = item->udata;

		assert(ctx->title && ctx->url);

		ctx->found = (*ctx->url && c_strcasestr(bm->url, ctx->url));
		if (!ctx->found && *ctx->title) {
			/* The comparison of bookmark titles should
			 * be case-insensitive and locale-sensitive
			 * (Turkish dotless i).  ELinks doesn't have
			 * such a function for UTF-8.  The best we
			 * have is strcasestr, which uses the system
			 * charset.  So convert bm->title to that.
			 * (ctx->title has already been converted.)  */
			struct conv_table *convert_table;
			unsigned char *title = NULL;

			convert_table = get_translation_table(ctx->utf8_cp,
							      ctx->system_cp);
			if (convert_table) {
				title = convert_string(convert_table,
						       bm->title,
						       strlen(bm->title),
						       ctx->system_cp,
						       CSM_NONE, NULL,
						       NULL, NULL);
			}

			if (title) {
				ctx->found = (strcasestr(title, ctx->title)
					      != NULL);
				mem_free(title);
			}
			/** @todo Tell the user that the string could
			 * not be converted.  */
		}

		if (ctx->found) *offset = 0;
	}
	ctx->offset++;

	return 0;
}

/* Last searched values.  Both are in UTF-8.  (The title could be kept
 * in the system charset, but that would be a bit risky, because
 * setlocale calls from Lua scripts can change the system charset.)  */
static unsigned char *bm_last_searched_title = NULL;
static unsigned char *bm_last_searched_url = NULL;

void
free_last_searched_bookmark(void)
{
	mem_free_set(&bm_last_searched_title, NULL);
	mem_free_set(&bm_last_searched_url, NULL);
}

static int
memorize_last_searched_bookmark(const unsigned char *title,
				const unsigned char *url)
{
	/* Memorize last searched title */
	mem_free_set(&bm_last_searched_title, stracpy(title));
	if (!bm_last_searched_title) return 0;

	/* Memorize last searched url */
	mem_free_set(&bm_last_searched_url, stracpy(url));
	if (!bm_last_searched_url) {
		mem_free_set(&bm_last_searched_title, NULL);
		return 0;
	}

	return 1;
}

/* Search bookmarks */
static void
bookmark_search_do(void *data)
{
	struct dialog *dlg = data;
	struct bookmark_search_ctx ctx = NULL_BOOKMARK_SEARCH_CTX;
	struct listbox_data *box;
	struct dialog_data *dlg_data;
	struct conv_table *convert_table;
	int term_cp;
	unsigned char *url_term;
	unsigned char *title_term;
	unsigned char *title_utf8 = NULL;

	assertm(dlg->udata != NULL, "Bookmark search with NULL udata in dialog");
	if_assert_failed return;

	dlg_data = (struct dialog_data *) dlg->udata;
	term_cp = get_terminal_codepage(dlg_data->win->term);
	ctx.system_cp = get_cp_index("System");
	ctx.utf8_cp = get_cp_index("UTF-8");

	title_term = dlg->widgets[0].data; /* need not be freed */
	url_term   = dlg->widgets[1].data; /* likewise */

	convert_table = get_translation_table(term_cp, ctx.system_cp);
	if (!convert_table) goto free_all;
	ctx.title = convert_string(convert_table,
				   title_term, strlen(title_term),
				   ctx.system_cp, CSM_NONE, NULL, NULL, NULL);
	if (!ctx.title) goto free_all;

	convert_table = get_translation_table(term_cp, ctx.utf8_cp);
	if (!convert_table) goto free_all;
	ctx.url = convert_string(convert_table,
				 url_term, strlen(url_term),
				 ctx.utf8_cp, CSM_NONE, NULL, NULL, NULL);
	if (!ctx.url) goto free_all;
	title_utf8 = convert_string(convert_table,
				    title_term, strlen(title_term),
				    ctx.utf8_cp, CSM_NONE, NULL, NULL, NULL);
	if (!title_utf8) goto free_all;

	if (!memorize_last_searched_bookmark(title_utf8, ctx.url))
		goto free_all;

	box = get_dlg_listbox_data(dlg_data);

	traverse_listbox_items_list(box->sel, box, 0, 0, test_search, &ctx);
	if (!ctx.found) goto free_all;

	listbox_sel_move(dlg_data->widgets_data, ctx.offset - 1);

free_all:
	mem_free_if(ctx.title);
	mem_free_if(ctx.url);
	mem_free_if(title_utf8);
}

static void
launch_bm_search_doc_dialog(struct terminal *term,
			    struct dialog_data *parent,
			    struct session *ses)
{
	unsigned char *title = NULL;
	unsigned char *url = NULL;

	if (bm_last_searched_title && bm_last_searched_url) {
		int utf8_cp, term_cp;
		struct conv_table *convert_table;

		utf8_cp = get_cp_index("UTF-8");
		term_cp = get_terminal_codepage(term);

		convert_table = get_translation_table(utf8_cp, term_cp);
		if (convert_table) {
			title = convert_string(convert_table, bm_last_searched_title,
					       strlen(bm_last_searched_title), term_cp,
					       CSM_NONE, NULL, NULL, NULL);
			url = convert_string(convert_table, bm_last_searched_url,
					     strlen(bm_last_searched_url), term_cp,
					     CSM_NONE, NULL, NULL, NULL);
		}
		if (!title || !url) {
			mem_free_set(&title, NULL);
			mem_free_set(&url, NULL);
		}
	}

	do_edit_dialog(term, 1, N_("Search bookmarks"),
		       title, url,
		       ses, parent, bookmark_search_do, NULL, NULL,
		       EDIT_DLG_SEARCH);

	mem_free_if(title);
	mem_free_if(url);
}



/****************************************************************************\
  Bookmark add dialog.
\****************************************************************************/

/* Adds the bookmark */
static void
bookmark_add_add(void *data)
{
	struct dialog *dlg = data;
	struct dialog_data *dlg_data = (struct dialog_data *) dlg->udata;
	struct terminal *term = dlg->udata2;

	do_add_bookmark(term, dlg_data, dlg->widgets[0].data, dlg->widgets[1].data);
}

/** Open a dialog box for adding a bookmark.
 *
 * @param term
 *   The terminal in which the dialog box should appear.
 *
 * @param parent
 *   The bookmark manager, or NULL if the user requested this action
 *   from somewhere else.
 *
 * @param ses
 *   If @a title or @a url is NULL, get defaults from the current
 *   document of @a ses.
 *
 * @param title
 *   The initial title of the new bookmark, in the encoding of @a term.
 *   NULL means use @a ses.
 *
 * @param url
 *   The initial URL of the new bookmark, in the encoding of @a term.
 *   NULL means use @a ses.  */
void
launch_bm_add_dialog(struct terminal *term,
		     struct dialog_data *parent,
		     struct session *ses,
		     unsigned char *title,
		     unsigned char *url)
{
	/* When the user eventually pushes the OK button, BFU calls
	 * bookmark_add_add() and gives it the struct dialog * as the
	 * void * parameter.  However, bookmark_add_add() also needs
	 * to know the struct terminal *, and there is no way to get
	 * that from struct dialog.  The other bookmark dialogs work
	 * around that by making dialog.udata point to the struct
	 * dialog_data of the bookmark manager, but the "Add bookmark"
	 * dialog can be triggered with ACT_MAIN_ADD_BOOKMARK, which
	 * does not involve the bookmark manager at all.
	 *
	 * The solution here is to save the struct terminal * in
	 * dialog.udata2, which the "Edit bookmark" dialog uses for
	 * struct bookmark *.  When adding a new bookmark, we don't
	 * need a pointer to an existing one, of course.  */
	do_edit_dialog(term, 1, N_("Add bookmark"), title, url, ses,
		       parent, bookmark_add_add, NULL, term, EDIT_DLG_ADD);
}

void
launch_bm_add_doc_dialog(struct terminal *term,
			 struct dialog_data *parent,
			 struct session *ses)
{
	launch_bm_add_dialog(term, parent, ses, NULL, NULL);
}

void
launch_bm_add_link_dialog(struct terminal *term,
			  struct dialog_data *parent,
			  struct session *ses)
{
	unsigned char title[MAX_STR_LEN], url[MAX_STR_LEN];

	launch_bm_add_dialog(term, parent, ses,
			     get_current_link_name(ses, title, MAX_STR_LEN),
			     get_current_link_url(ses, url, MAX_STR_LEN));
}


/****************************************************************************\
  Bookmark tabs dialog.
\****************************************************************************/

static void
bookmark_terminal_tabs_ok(void *term_void, unsigned char *foldername)
{
	struct terminal *const term = term_void;
	int from_cp = get_terminal_codepage(term);
	int to_cp = get_cp_index("UTF-8");
	struct conv_table *convert_table;
	unsigned char *converted;

	convert_table = get_translation_table(from_cp, to_cp);
	if (convert_table == NULL) return; /** @todo Report the error */

	converted = convert_string(convert_table,
				   foldername, strlen(foldername),
				   to_cp, CSM_NONE,
				   NULL, NULL, NULL);
	if (converted == NULL) return; /** @todo Report the error */

	bookmark_terminal_tabs(term_void, converted);
	mem_free(converted);
}

void
bookmark_terminal_tabs_dialog(struct terminal *term)
{
	struct string string;

	if (!init_string(&string)) return;

	add_to_string(&string, _("Saved session", term));

#ifdef HAVE_STRFTIME
	add_to_string(&string, " - ");
	add_date_to_string(&string, get_opt_str("ui.date_format"), NULL);
#endif

	input_dialog(term, NULL,
		     N_("Bookmark tabs"), N_("Enter folder name"),
		     term, NULL,
		     MAX_STR_LEN, string.source, 0, 0, NULL,
		     bookmark_terminal_tabs_ok, NULL);

	done_string(&string);
}
