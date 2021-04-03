
#ifndef EL__PROTOCOL_HTTP_CODES_H
#define EL__PROTOCOL_HTTP_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

struct connection;

/* HTTP response codes device. */

void http_error_document(struct connection *conn, int code);

#ifdef __cplusplus
}
#endif

#endif /* EL__PROTOCOL_HTTP_CODES_H */
