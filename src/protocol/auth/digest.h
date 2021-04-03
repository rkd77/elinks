
#ifndef EL__PROTOCOL_AUTH_DIGEST_H
#define EL__PROTOCOL_AUTH_DIGEST_H

#ifdef __cplusplus
extern "C" {
#endif

struct auth_entry;
struct uri;

char *
get_http_auth_digest_response(struct auth_entry *entry, struct uri *uri);

#ifdef __cplusplus
}
#endif

#endif
