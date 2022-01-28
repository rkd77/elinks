
#ifndef EL__PROTOCOL_HTTP_BLACKLIST_H
#define EL__PROTOCOL_HTTP_BLACKLIST_H

#ifdef __cplusplus
extern "C" {
#endif

struct uri;

enum blacklist_flags {
	SERVER_BLACKLIST_NONE = 0,
	SERVER_BLACKLIST_HTTP10 = 1,
	SERVER_BLACKLIST_NO_CHARSET = 2,
	SERVER_BLACKLIST_NO_TLS = 4,
	SERVER_BLACKLIST_NO_CERT_VERIFY = 8,
};

typedef unsigned char blacklist_flags_T;

void add_blacklist_entry(struct uri *, blacklist_flags_T);
void del_blacklist_entry(struct uri *, blacklist_flags_T);
blacklist_flags_T get_blacklist_flags(struct uri *);
void free_blacklist(void);

#ifdef __cplusplus
}
#endif

#endif
