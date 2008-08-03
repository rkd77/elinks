#ifndef EL__NETWORK_SOCKET_H
#define EL__NETWORK_SOCKET_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif

#include "network/state.h"

struct connect_info;
struct read_buffer;
struct socket;
struct uri;


/* Use internally for error return values. */
enum socket_error {
	SOCKET_SYSCALL_ERROR	= -1,	/* Retry with connection_state_for_errno(errno). */
	SOCKET_INTERNAL_ERROR	= -2,	/* Stop with connection_state(errno). */
	SOCKET_SSL_WANT_READ	= -3,	/* Try to read some more. */
	SOCKET_CANT_READ	= -4,	/* Retry with S_CANT_READ state. */
	SOCKET_CANT_WRITE	= -5,	/* Retry with S_CANT_WRITE state. */
};

enum socket_state {
	/* If a zero-byte message is read prematurely the connection will be
	 * retried with error state S_CANT_READ. */
	SOCKET_RETRY_ONCLOSE,
	/* If a zero-byte message is read flush the remaining bytes in the
	 * buffer and tell the protocol handler to end the reading by calling
	 * read_buffer->done(). */
	SOCKET_END_ONCLOSE,
	/* Used for signaling to protocols - via the read_buffer->done()
	 * callback - that a zero-byte message was read. */
	SOCKET_CLOSED,
};

typedef void (*socket_read_T)(struct socket *, struct read_buffer *);
typedef void (*socket_write_T)(struct socket *);
typedef void (*socket_connect_T)(struct socket *);
typedef void (*socket_operation_T)(struct socket *, struct connection_state);

struct socket_operations {
	/* Report change in the state of the socket. */
	socket_operation_T set_state;
	/* Reset the timeout for the socket. */
	socket_operation_T set_timeout;
	/* Some system related error occurred; advise to reconnect. */
	socket_operation_T retry;
	/* A fatal error occurred, like a memory allocation failure; advise to
	 * abort the connection. */
	socket_operation_T done;
};

struct read_buffer {
	/* A routine called *each time new data comes in*, therefore
	 * usually many times, not only when all the data arrives. */
	socket_read_T done;

	int length;
	int freespace;

	unsigned char data[1]; /* must be at end of struct */
};

struct socket {
	/* The socket descriptor */
	int fd;

	/* Somewhat read-specific socket state management and signaling. */
	enum socket_state state;

	/* Information for resolving the connection with which the socket is
	 * associated. */
	void *conn;

	/* Information used during the connection establishing phase. */
	struct connect_info *connect_info;

	/* Use for read and write buffers. */
	struct read_buffer *read_buffer;
	void *write_buffer;

	/* Callbacks to the connection management: */
	struct socket_operations *ops;

	/* Used by the request/response interface for saving the read_done
	 * operation. */
	socket_read_T read_done;

	/* For connections using SSL this is in fact (ssl_t *), but we don't
	 * want to know. Noone cares and inclusion of SSL header files costs a
	 * lot of compilation time. --pasky */
	void *ssl;

	unsigned int protocol_family:1; /* EL_PF_INET, EL_PF_INET6 */
	unsigned int need_ssl:1;	/* If the socket needs SSL support */
	unsigned int no_tls:1;		/* Internal SSL flag. */
	unsigned int set_no_tls:1;	/* Was the blacklist checked yet? */
	unsigned int duplex:1;		/* Allow simultaneous reads & writes. */
};

#define EL_PF_INET	0
#define EL_PF_INET6	1

/* Socket management: */

/* Allocate and setup a socket. */
struct socket *init_socket(void *conn, struct socket_operations *ops);

/* Reset a socket so it can be reused. XXX: Does not free() it! */
void done_socket(struct socket *socket);

/* Closes socket if open and clear any associated handlers. */
void close_socket(struct socket *socket);

/* Timeout handler. Will try to reconnect if possible. */
void timeout_socket(struct socket *socket);


/* Connection establishing: */

/* End successful connect() attempt to socket. */
void complete_connect_socket(struct socket *socket, struct uri *uri,
			     socket_connect_T done);

/* Establish connection with the host and port in @uri. Storing the socket
 * descriptor in @socket. When the connection has been established the @done
 * callback will be run. @no_cache specifies whether the DNS cache should be
 * ignored. */
void make_connection(struct socket *socket, struct uri *uri,
		     socket_connect_T connect_done, int no_cache);

/* Creates and returns a listening socket in the same IP family as the passed
 * ctrl_socket and stores info about the created socket in @addr. @ctrl_socket
 * is used for getting bind() information. */
int get_pasv_socket(struct socket *ctrl_socket, struct sockaddr_storage *addr);

/* Try to connect to the next available address or force the connection to retry
 * if all has already been tried. Updates the connection state to
 * @connection_state. */
void connect_socket(struct socket *socket, struct connection_state state);

/* Used by the SSL layer when negotiating. */
void dns_exception(struct socket *socket);


/* Reading and writing to sockets: */

/* Reads data from @socket into @buffer. Calls @done each time new data is
 * ready. */
void read_from_socket(struct socket *socket, struct read_buffer *buffer,
		      struct connection_state state, socket_read_T done);

/* Writes @datalen bytes from @data buffer to the passed @socket. When all data
 * is written the @done callback will be called. */
void write_to_socket(struct socket *socket,
		     unsigned char *data, int datalen,
		     struct connection_state state, socket_write_T write_done);

/* Send request and get response. */
void request_from_socket(struct socket *socket, unsigned char *data, int datalen,
			 struct connection_state state, enum socket_state sock_state,
			 socket_read_T read_done);

/* Initialize a read buffer. */
struct read_buffer *alloc_read_buffer(struct socket *socket);

/* Remove @bytes number of bytes from @buffer. */
void kill_buffer_data(struct read_buffer *buffer, int bytes);

#endif
