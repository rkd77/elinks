
#ifndef EL__PROTOCOL_AUTH_DIGEST_H
#define EL__PROTOCOL_AUTH_DIGEST_H

struct auth_entry;
struct uri;

unsigned char *
get_http_auth_digest_response(struct auth_entry *entry, struct uri *uri);

#endif
