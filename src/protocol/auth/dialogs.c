/* HTTP Auth dialog stuff */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


static void
auth_ok(void *data)
{
	struct dialog *dlg = data;
	struct auth_entry *entry = dlg->udata2;
	struct session *ses = dlg->udata;

	entry->blocked = 0;
	entry->valid = auth_entry_has_userinfo(entry);

	if (entry->valid && have_location(ses)) {
		struct location *loc = cur_loc(ses);
		struct uri *uri = loc->vs.uri;

		/* Make a 'fake' redirect to a URI without user/password so that
		 * the user/password from the URI will not override what the
		 * user just entered in the dialog. */
		if ((uri->userlen && strlcmp(entry->user, -1, uri->user, uri->userlen))
		    || (uri->password && strlcmp(entry->password, -1, uri->password, uri->passwordlen))) {

			uri = get_composed_uri(uri, URI_HTTP_AUTH | URI_DATA | URI_POST);
			if (uri) {
				goto_uri_frame(ses, uri, NULL, CACHE_MODE_INCREMENT);
				done_uri(uri);
				return;
			}
		}
	}

	reload(ses, CACHE_MODE_INCREMENT);
}

static void
auth_cancel(void *data)
{
	struct auth_entry *entry = data;

	entry->blocked = 0;
	del_auth_entry(entry);
}

/* TODO: Take auth_entry from data. --jonas */
void
do_auth_dialog(struct session *ses, void *data)
{
	/* [gettext_accelerator_context(do_auth_dialog)] */
	struct dialog *dlg;
	struct dialog_data *dlg_data;
	struct terminal *term = ses->tab->term;
	struct auth_entry *a = get_invalid_auth_entry();
	unsigned char sticker[MAX_STR_LEN], *text;
	int sticker_len;

	if (!a || a->blocked) return;

	text = get_uri_string(a->uri, URI_HTTP_AUTH);
	if (!text) return;

	sticker_len = snprintf(sticker, sizeof(sticker),
			       _("Authentication required for %s at %s", term),
			       a->realm, text);
	mem_free(text);
	if (sticker_len < 0 || sticker_len > MAX_STR_LEN) return;

#define AUTH_WIDGETS_COUNT 5
	/* + 1 to leave room for the '\0'. */
	dlg = calloc_dialog(AUTH_WIDGETS_COUNT, sticker_len + 1);
	if (!dlg) return;

	a->blocked = 1;

	/* This function is used for at least HTTP and FTP, so don't
	 * name the protocol here.  Consider also what an FTP server
	 * behind an HTTP proxy should be called.  */
	dlg->title = _("Authentication required", term);
	dlg->layouter = generic_dialog_layouter;

	text = get_dialog_offset(dlg, AUTH_WIDGETS_COUNT);
	memcpy(text, sticker, sticker_len); /* calloc_dialog has stored '\0' */

	dlg->udata = (void *) ses;
	dlg->udata2 = a;

	add_dlg_text(dlg, text, ALIGN_LEFT, 0);
	add_dlg_field_float(dlg, _("Login", term), 0, 0, NULL, AUTH_USER_MAXLEN, a->user, NULL);
	add_dlg_field_float_pass(dlg, _("Password", term), 0, 0, NULL, AUTH_PASSWORD_MAXLEN, a->password);

	add_dlg_ok_button(dlg, _("~OK", term), B_ENTER, auth_ok, dlg);
	add_dlg_ok_button(dlg, _("~Cancel", term), B_ESC, auth_cancel, a);

	add_dlg_end(dlg, AUTH_WIDGETS_COUNT);

	dlg_data = do_dialog(term, dlg, getml(dlg, (void *) NULL));
	/* When there's some username, but no password, automagically jump to
	 * the password. */
	if (dlg_data && a->user[0] && !a->password[0])
		select_widget_by_id(dlg_data, 1);
}


static void
lock_auth_entry(struct listbox_item *item)
{
	object_lock((struct auth_entry *) item->udata);
}

static void
unlock_auth_entry(struct listbox_item *item)
{
	object_unlock((struct auth_entry *) item->udata);
}

static int
is_auth_entry_used(struct listbox_item *item)
{
	return is_object_used((struct auth_entry *) item->udata);
}

static unsigned char *
get_auth_entry_text(struct listbox_item *item, struct terminal *term)
{
	struct auth_entry *auth_entry = item->udata;

	return get_uri_string(auth_entry->uri, URI_HTTP_AUTH);
}

static unsigned char *
get_auth_entry_info(struct listbox_item *item, struct terminal *term)
{
	struct auth_entry *auth_entry = item->udata;
	struct string info;

	if (item->type == BI_FOLDER) return NULL;
	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: ", _("URL", term));
	add_uri_to_string(&info, auth_entry->uri, URI_HTTP_AUTH);

	add_format_to_string(&info, "\n%s: ", _("Realm", term));
	if (auth_entry->realm) {
		int len = strlen(auth_entry->realm);
		int maxlen = 512; /* Max. number of chars displayed for realm. */

		if (len < maxlen)
			add_bytes_to_string(&info, auth_entry->realm, len);
		else {
			add_bytes_to_string(&info, auth_entry->realm, maxlen);
			add_to_string(&info, "...");
		}
	} else {
		add_to_string(&info, _("none", term));
	}

	add_format_to_string(&info, "\n%s: %s\n", _("State", term),
		auth_entry->valid ? _("valid", term) : _("invalid", term));

	return info.source;
}

static struct uri *
get_auth_entry_uri(struct listbox_item *item)
{
	struct auth_entry *auth_entry = item->udata;

	return get_composed_uri(auth_entry->uri, URI_HTTP_AUTH);
}

static struct listbox_item *
get_auth_entry_root(struct listbox_item *box_item)
{
	return NULL;
}

static int
can_delete_auth_entry(struct listbox_item *item)
{
	return 1;
}

static void
delete_auth_entry(struct listbox_item *item, int last)
{
	struct auth_entry *auth_entry = item->udata;

	assert(!is_object_used(auth_entry));

	del_auth_entry(auth_entry);
}

static struct listbox_ops_messages http_auth_messages = {
	/* cant_delete_item */
	N_("Sorry, but auth entry \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but auth entry \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Delete marked auth entries"),
	/* delete_marked_items */
	N_("Delete marked auth entries?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Delete auth entry"),
	/* delete_item; xgettext:c-format */
	N_("Delete this auth entry?"),
	/* clear_all_items_title */
	N_("Clear all auth entries"),
	/* clear_all_items_title */
	N_("Do you really want to remove all auth entries?"),
};

static const struct listbox_ops auth_listbox_ops = {
	lock_auth_entry,
	unlock_auth_entry,
	is_auth_entry_used,
	get_auth_entry_text,
	get_auth_entry_info,
	get_auth_entry_uri,
	get_auth_entry_root,
	NULL,
	can_delete_auth_entry,
	delete_auth_entry,
	NULL,
	&http_auth_messages,
};

static const struct hierbox_browser_button auth_buttons[] = {
	/* [gettext_accelerator_context(.auth_buttons)] */
	{ N_("~Goto"),   push_hierbox_goto_button,   1 },
	{ N_("~Info"),   push_hierbox_info_button,   1 },
	{ N_("~Delete"), push_hierbox_delete_button, 1 },
	{ N_("C~lear"),  push_hierbox_clear_button,  1 },
};

struct_hierbox_browser(
	auth_browser,
	N_("Authentication manager"),
	auth_buttons,
	&auth_listbox_ops
);

void
auth_manager(struct session *ses)
{
	hierbox_browser(&auth_browser, ses);
}
