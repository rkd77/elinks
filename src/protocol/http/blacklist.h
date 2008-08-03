
#ifndef EL__PROTOCOL_HTTP_BLACKLIST_H
#define EL__PROTOCOL_HTTP_BLACKLIST_H

struct uri;

enum blacklist_flags {
	SERVER_BLACKLIST_NONE = 0,
	SERVER_BLACKLIST_HTTP10 = 1,
	SERVER_BLACKLIST_NO_CHARSET = 2,
	SERVER_BLACKLIST_NO_TLS = 4,
};

void add_blacklist_entry(struct uri *, enum blacklist_flags);
void del_blacklist_entry(struct uri *, enum blacklist_flags);
enum blacklist_flags get_blacklist_flags(struct uri *);
void free_blacklist(void);

#endif
