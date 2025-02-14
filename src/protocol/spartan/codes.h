#ifndef EL__PROTOCOL_SPARTAN_CODES_H
#define EL__PROTOCOL_SPARTAN_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

struct connection;

/* Spartan response codes device. */

void spartan_error_document(struct connection *conn, const char *msg, int code);

#ifdef __cplusplus
}
#endif

#endif
