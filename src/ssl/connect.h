/* $Id: connect.h,v 1.13 2004/08/02 23:22:20 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef CONFIG_SSL

#include "lowlevel/connect.h"
#include "sched/connection.h"

int ssl_connect(struct connection *conn, struct connection_socket *socket);
int ssl_write(struct connection *conn, struct connection_socket *socket, unsigned char *data, int len);
int ssl_read(struct connection *conn, struct connection_socket *socket, struct read_buffer *);
int ssl_close(struct connection *conn, struct connection_socket *socket);

#endif
#endif
