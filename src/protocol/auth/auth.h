
#ifndef EL__PROTOCOL_AUTH_AUTH_H
#define EL__PROTOCOL_AUTH_AUTH_H

#include "main/object.h"
#include "protocol/uri.h"
#include "util/lists.h"

struct listbox_item;
struct module;

struct auth_entry {
	OBJECT_HEAD(struct auth_entry);

	/* Only the user, password and host part is supposed to be used in this
	 * URI. It is mainly a convenient way to store host info so later when
	 * finding auth entries it can be compared as a reference against the
	 * current URI that needs to send authorization. */
	struct uri *uri;

	unsigned char *realm;
	unsigned char *nonce;
	unsigned char *opaque;

	struct listbox_item *box_item;

	unsigned char user[AUTH_USER_MAXLEN];
	unsigned char password[AUTH_PASSWORD_MAXLEN];

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

extern struct module auth_module;

#endif
