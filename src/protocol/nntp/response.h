
#ifndef EL__PROTOCOL_NNTP_RESPONSE_H
#define EL__PROTOCOL_NNTP_RESPONSE_H

#include "protocol/nntp/connection.h"

struct connection;
struct read_buffer;

/* Reads multi-lined response data */
/* Slightly abusive usage of connection state to signal what is going on
 * or what went wrong:
 * S_TRANS (transferring) means 'end-of-text' not reached yet
 * S_OK 		  means no more text expected
 * S_OUT_OF_MEM		  allocation failure of some sort */
struct connection_state
read_nntp_response_data(struct connection *conn, struct read_buffer *rb);

/* Reads the first line in the NNTP response from the @rb read buffer and
 * returns the response code. */
/* Returns:
 * NNTP_CODE_NONE	if no \r\n ended line could be read and another
 *			check should be rescheduled.
 * NNTP_CODE_INVALID	if the response code is not within the range
 *			100 - 599 of valid codes. */
enum nntp_code
get_nntp_response_code(struct connection *conn, struct read_buffer *rb);

#endif
