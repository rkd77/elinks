/* HTTP Authentication support */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/hierbox.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/dialogs.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Defines to 1 to enable http auth debugging output. */
#if 0
#define DEBUG_HTTP_AUTH
#endif

static INIT_LIST_OF(struct auth_entry, auth_entry_list);


/* Find if url/realm is in auth list. If a matching url is found, but realm is
 * NULL, it returns the first record found. If realm isn't NULL, it returns
 * the first record that matches exactly (url and realm) if any. */
static struct auth_entry *
find_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct auth_entry *match = NULL, *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("find_auth_entry: url=%s realm=%s", url, realm);
#endif

	foreach (entry, auth_entry_list) {
		if (!compare_uri(entry->uri, uri, URI_HTTP_AUTH))
			continue;

		/* Found a matching url. */
		match = entry;
		if (!realm) {
			/* Since realm is NULL, stops immediatly. */
			break;
		}

		/* From RFC 2617 section 1.2:
		 *
		 * The realm value (case-sensitive), in combination with the
		 * canonical root URL (the absolute URI for the server whose
		 * abs_path is empty; see section 5.1.2 of [2]) of the server
		 * being accessed, defines the protection space. */
		if (entry->realm && !strcmp(entry->realm, realm)) {
			/* Exact match. */
			break; /* Stop here. */
		}
	}

	return match;
}

static void
set_auth_user(struct auth_entry *entry, struct uri *uri)
{
	int userlen = int_min(uri->userlen, AUTH_USER_MAXLEN - 1);

	if (userlen)
		memcpy(entry->user, uri->user, userlen);

	entry->user[userlen] = '\0';
}

static void
set_auth_password(struct auth_entry *entry, struct uri *uri)
{
	int passwordlen = int_min(uri->passwordlen, AUTH_PASSWORD_MAXLEN - 1);

	if (passwordlen)
		memcpy(entry->password, uri->password, passwordlen);

	entry->password[passwordlen] = '\0';
}

static void done_auth_entry(struct auth_entry *entry);

static struct auth_entry *
init_auth_entry(struct uri *uri, unsigned char *realm)
{
	struct auth_entry *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("init_auth_entry: auth_url=%s realm=%s uri=%p", auth_url, realm, uri);
#endif

	entry = mem_calloc(1, sizeof(*entry));
	if (!entry) return NULL;

	entry->uri = get_uri_reference(uri);

	if (realm) {
		/* Copy realm value. */
		entry->realm = stracpy(realm);
		if (!entry->realm) {
			mem_free(entry);
			return NULL;
		}
	}

	/* Copy user and pass info passed url if any else NULL terminate. */

	set_auth_user(entry, uri);
	set_auth_password(entry, uri);

	entry->box_item = add_listbox_leaf(&auth_browser, NULL, entry);
	if (!entry->box_item) {
		done_auth_entry(entry);
		return NULL;
	}

	return entry;
}

/* Add a Basic Auth entry if needed. */
/* Returns the new entry or updates an existing one. Sets the @valid member if
 * updating is required so it can be tested if the user should be queried. */
struct auth_entry *
add_auth_entry(struct uri *uri, unsigned char *realm, unsigned char *nonce,
	unsigned char *opaque, unsigned int digest)
{
	struct auth_entry *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("add_auth_entry: newurl=%s realm=%s uri=%p", newurl, realm, uri);
#endif

	/* Is host/realm already known ? */
	entry = find_auth_entry(uri, realm);
	if (entry) {
		/* Waiting for user/pass in dialog. */
		if (entry->blocked) return NULL;

		/* In order to use an existing entry it has to match exactly.
		 * This is done step by step. If something isn't equal the
		 * entry is updated and marked as invalid. */

		/* If only one realm is defined or they don't compare. */
		if ((!!realm ^ !!entry->realm)
		    || (realm && entry->realm && strcmp(realm, entry->realm))) {
			entry->valid = 0;
			mem_free_set(&entry->realm, NULL);
			if (realm) {
				entry->realm = stracpy(realm);
				if (!entry->realm) {
					del_auth_entry(entry);
					return NULL;
				}
				if (nonce) {
					mem_free_set(&entry->nonce, stracpy(nonce));
					if (!entry->nonce) {
						del_auth_entry(entry);
						return NULL;
					}
				}
				if (opaque) {
					mem_free_set(&entry->opaque, stracpy(opaque));
					if (!entry->opaque) {
						del_auth_entry(entry);
						return NULL;
					}
				}
				entry->digest = digest;
			}
		}

		if (!*entry->user
		    || (!uri->user || !uri->userlen ||
			strlcmp(entry->user, -1, uri->user, uri->userlen))) {
			entry->valid = 0;
			set_auth_user(entry, uri);
		}

		if (!*entry->password
		    || (!uri->password || !uri->passwordlen ||
			strlcmp(entry->password, -1, uri->password, uri->passwordlen))) {
			entry->valid = 0;
			set_auth_password(entry, uri);
		}

	} else {
		/* Create a new entry. */
		entry = init_auth_entry(uri, realm);
		if (!entry) return NULL;
		add_to_list(auth_entry_list, entry);
		if (nonce) {
			entry->nonce = stracpy(nonce);
			if (!entry->nonce) {
				del_auth_entry(entry);
				return NULL;
			}
		}
		if (opaque) {
			entry->opaque = stracpy(opaque);
			if (!entry->opaque) {
				del_auth_entry(entry);
				return NULL;
			}
		}
		entry->digest = digest;
	}

	/* Only pop up question if one of the protocols requested it */
	if (entry && !entry->valid && entry->realm)
		add_questions_entry(do_auth_dialog, entry);

	return entry;
}

/* Find an entry in auth list by url. If url contains user/pass information
 * and entry does not exist then entry is created.
 * If entry exists but user/pass passed in url is different, then entry is
 * updated (but not if user/pass is set in dialog). */
/* It returns a base 64 encoded user + pass suitable to use in Authorization
 * header, or NULL on failure. */
struct auth_entry *
find_auth(struct uri *uri)
{
	struct auth_entry *entry = NULL;

#ifdef DEBUG_HTTP_AUTH
	DBG("find_auth: newurl=%s uri=%p", newurl, uri);
#endif

	entry = find_auth_entry(uri, NULL);
	/* Check is user/pass info is in url. */
	if (uri->userlen || uri->passwordlen) {
		/* Add a new entry either to save the user/password info from the URI
		 * so it is available if we later get redirected to a URI with
		 * the user/password stripped. Else if update with entry with
		 * the user/password from the URI. */
		if (!entry
		    || (uri->userlen && strlcmp(entry->user, -1, uri->user, uri->userlen))
		    || (uri->password && strlcmp(entry->password, -1, uri->password, uri->passwordlen))) {

			entry = add_auth_entry(uri, NULL, NULL, NULL, 0);
		}
	}

	/* No entry found or waiting for user/password in dialog. */
	if (!entry || entry->blocked)
		return NULL;

	/* Sanity check. */
	if (!auth_entry_has_userinfo(entry)) {
		del_auth_entry(entry);
		return NULL;
	}

	return entry;
}

static void
done_auth_entry(struct auth_entry *entry)
{
	if (entry->box_item)
		done_listbox_item(&auth_browser, entry->box_item);
	done_uri(entry->uri);
	mem_free_if(entry->realm);
	mem_free_if(entry->nonce);
	mem_free_if(entry->opaque);
	mem_free(entry);
}

/* Delete an entry from auth list. */
void
del_auth_entry(struct auth_entry *entry)
{
#ifdef DEBUG_HTTP_AUTH
	DBG("del_auth_entry: url=%s realm=%s user=%p",
	      entry->url, entry->realm, entry->user);
#endif

	del_from_list(entry);

	done_auth_entry(entry);
}

/* Free all entries in auth list and questions in queue. */
void
free_auth(void)
{
#ifdef DEBUG_HTTP_AUTH
	DBG("free_auth");
#endif

	while (!list_empty(auth_entry_list))
		del_auth_entry(auth_entry_list.next);

	free_list(questions_queue);
}

static void
done_auth(struct module *xxx)
{
	free_auth();
}

struct auth_entry *
get_invalid_auth_entry(void)
{
	struct auth_entry *entry;

#ifdef DEBUG_HTTP_AUTH
	DBG("get_invalid_auth_entry");
#endif

	foreach (entry, auth_entry_list)
		if (!entry->valid)
			return entry;

	return NULL;
}

struct module auth_module = struct_module(
	/* name: */		N_("Authentication"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_auth
);
