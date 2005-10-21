
#ifndef EL__PROTOCOL_HTTP_CODES_H
#define EL__PROTOCOL_HTTP_CODES_H

struct connection;

/* HTTP response codes device. */

void http_error_document(struct connection *conn, int code);

#endif /* EL__PROTOCOL_HTTP_CODES_H */
