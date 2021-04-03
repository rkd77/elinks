#ifndef EL__COOKIES_COOKIES_H
#define EL__COOKIES_COOKIES_H

/* ELinks cookies file format:
 * NAME\tVALUE\tSERVER\tPATH\tDOMAIN\tEXPIRES\tSECURE\n
 *
 * \t is a tabulator
 * \n is a newline
 * EXPIRES is the number of seconds since 1970-01-01 00:00:00 UTC.
 * SECURE is 0 for http and 1 for https.
 */

#include "main/module.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "util/string.h"
#include "util/time.h"

#ifdef __cplusplus
extern "C" {
#endif

struct listbox_item;
struct terminal;

enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

struct cookie_server {
	OBJECT_HEAD(struct cookie_server);

	struct listbox_item *box_item;
	char host[1]; /* Must be at end of struct. */
};

struct cookie {
	OBJECT_HEAD(struct cookie);

	char *name, *value;
	char *path, *domain;

	struct cookie_server *server;	/* The host the cookie originated from */
	time_t expires;			/* Expiration time. Zero means undefined */
	unsigned int secure:1;			/* Did it have 'secure' attribute */
	unsigned int httponly:1;		/* Did it have 'httponly' attribute */

	struct listbox_item *box_item;
};

struct cookie_server *get_cookie_server(char *host, int hostlen);
struct cookie *init_cookie(char *name, char *value,
			   char *path, char *domain,
			   struct cookie_server *server);
void accept_cookie(struct cookie *);
void done_cookie(struct cookie *);
void delete_cookie(struct cookie *);
void set_cookie(struct uri *, char *);
void load_cookies(void);
void save_cookies(struct terminal *);
void set_cookies_dirty(void);

/* Note that the returned value points to a static structure and thus the
 * string will be overwritten at the next call time. The string source
 * itself is dynamically allocated, though. */
struct string *send_cookies(struct uri *uri);
struct string *send_cookies_js(struct uri *uri);

extern struct module cookies_module;

#ifdef __cplusplus
}
#endif

#endif
