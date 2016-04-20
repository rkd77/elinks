#ifndef EL__NETWORK_CONNECTION_H
#define EL__NETWORK_CONNECTION_H

#include "cache/cache.h"
#include "encoding/encoding.h"
#include "main/timer.h" /* timer_id_T */
#include "network/state.h"
#include "util/lists.h"
#include <stdio.h>

struct download;
struct socket;
struct uri;


struct connection {
	LIST_HEAD(struct connection);

	LIST_OF(struct download) downloads;
	struct progress *progress;

	/** Progress of sending the request and attached files to the
	 * server.  This happens before any download.
	 *
	 * Currently, ELinks supports file uploads only in HTTP and
	 * local CGI.  Therefore, upload_stat_timer() in connection.c
	 * assumes that #info points to struct http_connection_info
	 * whenever @c http_upload_progress is not NULL.  */
	struct progress *http_upload_progress;

	/* If no proxy is used uri and proxied_uri are the same. */
	struct uri *uri;
	struct uri *proxied_uri;
	struct uri *referrer;

	/* Cache information. */
	enum cache_mode cache_mode;
	struct cache_entry *cached;

	off_t from;		/* Position for new data in the cache entry. */
	off_t received;		/* The number of received bytes. */
	off_t est_length;	/* Estimated number of bytes to transfer. */

	enum stream_encoding content_encoding;
	struct stream_encoded *stream;

	/* Called if non NULL when shutting down a connection. */
	void (*done)(struct connection *);

	unsigned int id;

	struct connection_state state;
	struct connection_state prev_error;

	/* The communication socket with the other side. */
	struct socket *socket;
	/* The data socket. It is used, when @socket is used for the control,
	 * and the actual data is transmitted through a different channel. */
	/* The only users now are FTP, FSP, and SMB. */
	struct socket *data_socket;

	int tries;
	timer_id_T timer;

	unsigned int running:1;
	unsigned int unrestartable:1;
	unsigned int detached:1;
	unsigned int cgi:1;

	/* Each document is downloaded with some priority. When downloading a
	 * document, the existing connections are checked to see if a
	 * connection to the host already exists before creating a new one.  If
	 * it finds out that something had that idea earlier and connection for
	 * download of the very same URL is active already, it just attaches
	 * the struct download it got to the connection, _and_ updates its @pri
	 * array by the priority it has thus, sum of values in all fields of
	 * @pri is also kinda refcount of the connection. */
	int pri[PRIORITIES];

	/* Private protocol specific info. If non-NULL it is free()d when
	 * stopping the connection. */
	void *info;
};

int register_check_queue(void);

int get_connections_count(void);
int get_keepalive_connections_count(void);
int get_connections_connecting_count(void);
int get_connections_transfering_count(void);

void set_connection_state(struct connection *, struct connection_state);

int has_keepalive_connection(struct connection *);
void add_keepalive_connection(struct connection *conn, long timeout_in_seconds,
			      void (*done)(struct connection *));

void abort_connection(struct connection *, struct connection_state);
void retry_connection(struct connection *, struct connection_state);

void cancel_download(struct download *download, int interrupt);
void move_download(struct download *old, struct download *new_,
		     enum connection_priority newpri);

void detach_connection(struct download *, off_t);
void abort_all_connections(void);
void abort_background_connections(void);

void set_connection_timeout(struct connection *);

void shutdown_connection_stream(struct connection *conn);

/* Initiates a connection to get the given @uri. */
/* Note that stat's data _MUST_ be struct file_download * if start > 0! Yes,
 * that should be probably something else than data, but... ;-) */
/* Returns 0 on success and -1 on failure. */
int load_uri(struct uri *uri, struct uri *referrer, struct download *download,
	     enum connection_priority pri, enum cache_mode cache_mode, off_t start);

int is_entry_used(struct cache_entry *cached);

#endif
