/* Cache-related dialogs */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cache/dialogs.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static void
lock_cache_entry(struct listbox_item *item)
{
	object_lock((struct cache_entry *) item->udata);
}

static void
unlock_cache_entry(struct listbox_item *item)
{
	object_unlock((struct cache_entry *) item->udata);
}

static int
is_cache_entry_used(struct listbox_item *item)
{
	return is_object_used((struct cache_entry *) item->udata);
}

static unsigned char *
get_cache_entry_text(struct listbox_item *item, struct terminal *term)
{
	struct cache_entry *cached = item->udata;

	return get_uri_string(cached->uri, URI_PUBLIC);
}

static unsigned char *
get_cache_entry_info(struct listbox_item *item, struct terminal *term)
{
	struct cache_entry *cached = item->udata;
	struct string msg;

	if (item->type == BI_FOLDER) return NULL;
	if (!init_string(&msg)) return NULL;

	add_to_string(&msg, _("URL", term));
	add_to_string(&msg, ": ");
	add_uri_to_string(&msg, cached->uri, URI_PUBLIC);

	/* No need to use compare_uri() here we only want to check whether they
	 * point to the same URI. */
	if (cached->proxy_uri != cached->uri) {
		add_format_to_string(&msg, "\n%s: ", _("Proxy URL", term));
		add_uri_to_string(&msg, cached->proxy_uri, URI_PUBLIC);
	}

	if (cached->redirect) {
		add_format_to_string(&msg, "\n%s: ", _("Redirect", term));
		add_uri_to_string(&msg, cached->redirect, URI_PUBLIC);

		if (cached->redirect_get) {
			add_to_string(&msg, " (GET)");
		}
	}

	add_format_to_string(&msg, "\n%s: %" OFF_PRINT_FORMAT, _("Size", term),
			     (off_print_T) cached->length);
	add_format_to_string(&msg, "\n%s: %" OFF_PRINT_FORMAT, _("Loaded size", term),
			     (off_print_T) cached->data_size);
	if (cached->content_type) {
		add_format_to_string(&msg, "\n%s: %s", _("Content type", term),
				     cached->content_type);
	}
	if (cached->last_modified) {
		add_format_to_string(&msg, "\n%s: %s", _("Last modified", term),
				     cached->last_modified);
	}
	if (cached->etag) {
		add_format_to_string(&msg, "\n%s: %s", "ETag",
						cached->etag);
	}
	if (cached->ssl_info) {
		add_format_to_string(&msg, "\n%s: %s", _("SSL Cipher", term),
						cached->ssl_info);
	}
	if (cached->encoding_info) {
		add_format_to_string(&msg, "\n%s: %s", _("Encoding", term),
						cached->encoding_info);
	}

	if (cached->incomplete || !cached->valid) {
		add_char_to_string(&msg, '\n');
		add_to_string(&msg, _("Flags", term));
		add_to_string(&msg, ": ");
		if (cached->incomplete) {
			add_to_string(&msg, _("incomplete", term));
			add_char_to_string(&msg, ' ');
		}
		if (!cached->valid) add_to_string(&msg, _("invalid", term));
	}

#ifdef HAVE_STRFTIME
	if (cached->expire) {
		time_t expires = timeval_to_seconds(&cached->max_age);

		add_format_to_string(&msg, "\n%s: ", _("Expires", term));
		add_date_to_string(&msg, get_opt_str("ui.date_format"), &expires);
	}
#endif
#ifdef CONFIG_DEBUG
	add_format_to_string(&msg, "\n%s: %d", "Refcount", get_object_refcount(cached));
	add_format_to_string(&msg, "\n%s: %u", _("ID", term), cached->cache_id);

	if (cached->head && *cached->head) {
		add_format_to_string(&msg, "\n%s:\n\n%s", _("Header", term),
				     cached->head);
	}
#endif

	return msg.source;
}

static struct uri *
get_cache_entry_uri(struct listbox_item *item)
{
	struct cache_entry *cached = item->udata;

	return get_uri_reference(cached->uri);
}

static struct listbox_item *
get_cache_entry_root(struct listbox_item *item)
{
	return NULL;
}

static int
can_delete_cache_entry(struct listbox_item *item)
{
	return 1;
}

static void
delete_cache_entry_item(struct listbox_item *item, int last)
{
	struct cache_entry *cached = item->udata;

	assert(!is_object_used(cached));

	delete_cache_entry(cached);
}

static enum listbox_match
match_cache_entry(struct listbox_item *item, struct terminal *term,
		  unsigned char *text)
{
	struct cache_entry *cached = item->udata;

	if (c_strcasestr(struri(cached->uri), text)
	    || (cached->head && c_strcasestr(cached->head, text)))
		return LISTBOX_MATCH_OK;

	return LISTBOX_MATCH_NO;
}

static struct listbox_ops_messages cache_messages = {
	/* cant_delete_item */
	N_("Sorry, but cache entry \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but cache entry \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Delete marked cache entries"),
	/* delete_marked_items */
	N_("Delete marked cache entries?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Delete cache entry"),
	/* delete_item; xgettext:c-format */
	N_("Delete this cache entry?"),
	/* clear_all_items_title */
	NULL,
	/* clear_all_items_title */
	NULL,
};

static const struct listbox_ops cache_entry_listbox_ops = {
	lock_cache_entry,
	unlock_cache_entry,
	is_cache_entry_used,
	get_cache_entry_text,
	get_cache_entry_info,
	get_cache_entry_uri,
	get_cache_entry_root,
	match_cache_entry,
	can_delete_cache_entry,
	delete_cache_entry_item,
	NULL,
	&cache_messages,
};

static const struct hierbox_browser_button cache_buttons[] = {
	/* [gettext_accelerator_context(.cache_buttons)] */
	{ N_("~Info"),   push_hierbox_info_button,   1 },
	{ N_("~Goto"),   push_hierbox_goto_button,   1 },
	{ N_("~Delete"), push_hierbox_delete_button, 1 },
	{ N_("~Search"), push_hierbox_search_button, 1 },
};

struct_hierbox_browser(
	cache_browser,
	N_("Cache manager"),
	cache_buttons,
	&cache_entry_listbox_ops
);

void
cache_manager(struct session *ses)
{
	hierbox_browser(&cache_browser, ses);
}
