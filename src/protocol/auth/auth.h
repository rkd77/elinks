/* $Id: auth.h,v 1.29 2004/11/20 04:30:06 jonas Exp $ */

#ifndef EL__PROTOCOL_AUTH_AUTH_H
#define EL__PROTOCOL_AUTH_AUTH_H

#include "protocol/uri.h"
#include "util/object.h"
#include "util/lists.h"

struct listbox_item;

struct auth_entry {
	LIST_HEAD(struct auth_entry);

	/* Only the user, password and host part is supposed to be used in this
	 * URI. It is mainly a convenient way to store host info so later when
	 * finding auth entries it can be compared as a reference against the
	 * current URI that needs to send authorization. */
	struct uri *uri;

	unsigned char *realm;
	unsigned char *nonce;
	unsigned char *opaque;

	struct listbox_item *box_item;
	struct object object;

	unsigned char user[HTTP_AUTH_USER_MAXLEN];
	unsigned char password[HTTP_AUTH_PASSWORD_MAXLEN];

	unsigned int blocked:1;	/* A dialog is asking user for validation */
	unsigned int valid:1;	/* The entry has been validated by user */
	unsigned int digest:1;	/* It is an HTTP Digest entry */
};

#define auth_entry_has_userinfo(_entry_) \
	(*(_entry_)->user || *(_entry_)->password)

struct auth_entry *find_auth(struct uri *);
struct auth_entry *add_auth_entry(struct uri *, unsigned char *,
	unsigned char *, unsigned char *, unsigned int);
void del_auth_entry(struct auth_entry *);
void free_auth(void);
struct auth_entry *get_invalid_auth_entry(void);

#endif
