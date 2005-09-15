/* $Id: codes.h,v 1.5 2004/06/28 02:27:20 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_CODES_H
#define EL__PROTOCOL_HTTP_CODES_H

struct connection;

/* HTTP response codes device. */

void http_error_document(struct connection *conn, int code);

#endif /* EL__PROTOCOL_HTTP_CODES_H */
