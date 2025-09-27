/* Internal "mailcap's copiousoutput" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "cookies/cookies.h"
#include "intl/libintl.h"
#include "mime/backend/common.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/sysname.h"
#include "protocol/common.h"
#include "protocol/file/mailcap.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/env.h"
#include "util/string.h"

static union option_info mailcap_options[] = {
	INIT_OPT_TREE("protocol", N_("Mailcap"),
		"mailcap", OPT_ZERO,
		N_("Options specific to mailcap.")),

	INIT_OPT_BOOL("protocol.mailcap", N_("Allow empty referrer"),
		"allow_empty_referrer", OPT_ZERO, 0,
		N_("Whether to allow empty referrer for mailcap protocol. "
		"It can be helpful for restoring sessions, but it is a bit dangerous. "
		"When allowed you can execute any command in goto url dialog. "
		"For example mailcap:ls")),

	NULL_OPTION_INFO,
};


struct module mailcap_protocol_module = struct_module(
	/* name: */		N_("Mailcap"),
	/* options: */		mailcap_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL,
	/* getname: */	NULL
);

struct module mailcap_html_protocol_module = struct_module(
	/* name: */		N_("Mailcap html"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL,
	/* getname: */	NULL
);

#ifdef HAVE_FORK
static void
get_request(struct connection *conn, int x_htmloutput)
{
	ELOG
	struct read_buffer *rb = alloc_read_buffer(conn->socket);

	if (!rb) return;

	if (x_htmloutput) {
		memcpy(rb->data, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n", 44);
		rb->length = 44;
		rb->freespace -= 44;
	} else {
		memcpy(rb->data, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n", 45);
		rb->length = 45;
		rb->freespace -= 45;
	}

	conn->unrestartable = 1;

	conn->socket->state = SOCKET_END_ONCLOSE;
	read_from_socket(conn->socket, rb, connection_state(S_SENT),
			 http_got_header);
}
#endif

static void
mailcap_protocol_handler_common(struct connection *conn, int html)
{
	ELOG
#ifdef HAVE_FORK
	char *script;
	pid_t pid;
	struct connection_state state = connection_state(S_OK);
	int pipe_read[2];

	/* security checks */

	if (!conn->referrer && !get_opt_bool("protocol.mailcap.allow_empty_referrer", NULL)) {
		goto bad;
	}

	if (conn->referrer && (conn->referrer->protocol != PROTOCOL_MAILCAP && conn->referrer->protocol != PROTOCOL_MAILCAP_HTML)) {
		goto bad;
	}
	script = get_uri_string(conn->uri, URI_DATA);

	if (!script) {
		state = connection_state(S_OUT_OF_MEM);
		goto end2;
	}

	if (c_pipe(pipe_read)) {
		state = connection_state_for_errno(errno);
		goto end1;
	}

	pid = fork();
	if (pid < 0) {
		state = connection_state_for_errno(errno);
		goto end0;
	}
	if (!pid) {
		if (dup2(pipe_read[1], STDOUT_FILENO) < 0) {
			_exit(2);
		}
		close_all_non_term_fd();
		close(STDERR_FILENO);

		if (execl("/bin/sh", "/bin/sh", "-c", script, (char *) NULL)) {
			_exit(3);
		}

	} else { /* ELinks */
		mem_free(script);

		if (!init_http_connection_info(conn, 1, 0, 1)) {
			close(pipe_read[0]); close(pipe_read[1]);
			return;
		}

		close(pipe_read[1]);
		conn->socket->fd = pipe_read[0];

		conn->data_socket->fd = -1;
		conn->cgi = 1;
		set_nonblocking_fd(conn->socket->fd);

		get_request(conn, html);
		return;
	}

end0:
	close(pipe_read[0]); close(pipe_read[1]);
end1:
	mem_free(script);
end2:
	abort_connection(conn, state);
	return;
bad:
#endif
	abort_connection(conn, connection_state(S_BAD_URL));
}

void
mailcap_protocol_handler(struct connection *conn)
{
	ELOG
	mailcap_protocol_handler_common(conn, 0);
}

void
mailcap_html_protocol_handler(struct connection *conn)
{
	ELOG
	mailcap_protocol_handler_common(conn, 1);
}
