/* Internal "cgi" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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
#include "intl/gettext/libintl.h"
#include "mime/backend/common.h"
#include "network/connection.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/sysname.h"
#include "protocol/common.h"
#include "protocol/file/cgi.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/env.h"
#include "util/string.h"

static union option_info cgi_options[] = {
	INIT_OPT_TREE("protocol.file", N_("Local CGI"),
		"cgi", 0,
		N_("Local CGI specific options.")),

	INIT_OPT_STRING("protocol.file.cgi", N_("Path"),
		"path", 0, "",
		N_("Colon separated list of directories, "
		"where CGI scripts are stored.")),

	INIT_OPT_BOOL("protocol.file.cgi", N_("Allow local CGI"),
		"policy", 0, 0,
		N_("Whether to execute local CGI scripts.")),
	NULL_OPTION_INFO,
};

struct module cgi_protocol_module = struct_module(
	/* name: */		N_("CGI"),
	/* options: */		cgi_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

static void
close_pipe_and_read(struct socket *data_socket)
{
	struct connection *conn = data_socket->conn;
	struct read_buffer *rb = alloc_read_buffer(conn->socket);

	if (!rb) return;

	memcpy(rb->data, "HTTP/1.0 200 OK\r\n", 17);
	rb->length = 17;
	rb->freespace -= 17;

	conn->unrestartable = 1;
	close(data_socket->fd);
	data_socket->fd = -1;

	conn->socket->state = SOCKET_END_ONCLOSE;
	read_from_socket(conn->socket, rb, connection_state(S_SENT),
			 http_got_header);
}

static void
send_post_data(struct connection *conn)
{
#define POST_BUFFER_SIZE 4096
	unsigned char *post = conn->uri->post;
	unsigned char *postend;
	unsigned char buffer[POST_BUFFER_SIZE];
	struct string data;
	int n = 0;

	if (!init_string(&data)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	postend = strchr(post, '\n');
	if (postend) post = postend + 1;

	/* FIXME: Code duplication with protocol/http/http.c! --witekfl */
	while (post[0] && post[1]) {
		int h1, h2;

		h1 = unhx(post[0]);
		assert(h1 >= 0 && h1 < 16);
		if_assert_failed h1 = 0;

		h2 = unhx(post[1]);
		assert(h2 >= 0 && h2 < 16);
		if_assert_failed h2 = 0;

		buffer[n++] = (h1<<4) + h2;
		post += 2;
		if (n == POST_BUFFER_SIZE) {
			add_bytes_to_string(&data, buffer, n);
			n = 0;
		}
	}
	if (n)
		add_bytes_to_string(&data, buffer, n);


	/* If we're submitting a form whose controls do not have
	 * names, then the POST has a Content-Type but empty data,
	 * and an assertion would fail in write_to_socket.  */
	if (data.length)
		write_to_socket(conn->data_socket, data.source, data.length,
				connection_state(S_SENT), close_pipe_and_read);
	else
		close_pipe_and_read(conn->data_socket);

	done_string(&data);
#undef POST_BUFFER_SIZE
}

static void
send_request(struct connection *conn)
{
	if (conn->uri->post) send_post_data(conn);
	else close_pipe_and_read(conn->data_socket);
}

/* This function sets CGI environment variables. */
static int
set_vars(struct connection *conn, unsigned char *script)
{
	unsigned char *post = conn->uri->post;
	unsigned char *query = get_uri_string(conn->uri, URI_QUERY);
	unsigned char *str;
	int res = env_set("QUERY_STRING", empty_string_or_(query), -1);

	mem_free_if(query);
	if (res) return -1;

	if (post) {
		unsigned char *postend = strchr(post, '\n');
		unsigned char buf[16];

		if (postend) {
			res = env_set("CONTENT_TYPE", post, postend - post);
			if (res) return -1;
			post = postend + 1;
		}
		snprintf(buf, 16, "%d", (int) strlen(post) / 2);
		if (env_set("CONTENT_LENGTH", buf, -1)) return -1;
	}

	if (env_set("REQUEST_METHOD", post ? "POST" : "GET", -1)) return -1;
	if (env_set("SERVER_SOFTWARE", "ELinks/" VERSION, -1)) return -1;
	if (env_set("SERVER_PROTOCOL", "HTTP/1.0", -1)) return -1;
	/* XXX: Maybe it is better to set this to an empty string? --pasky */
	if (env_set("SERVER_NAME", "localhost", -1)) return -1;
	/* XXX: Maybe it is better to set this to an empty string? --pasky */
	if (env_set("REMOTE_ADDR", "127.0.0.1", -1)) return -1;
	if (env_set("GATEWAY_INTERFACE", "CGI/1.1", -1)) return -1;
	/* This is the path name extracted from the URI and decoded, per
	 * http://cgi-spec.golux.com/draft-coar-cgi-v11-03-clean.html#8.1 */
	if (env_set("SCRIPT_NAME", script, -1)) return -1;
	if (env_set("SCRIPT_FILENAME", script, -1)) return -1;
	if (env_set("PATH_TRANSLATED", script, -1)) return -1;
	if (env_set("REDIRECT_STATUS", "1", -1)) return -1;

	/* From now on, just HTTP-like headers are being set. Missing variables
	 * due to full environment are not a problem according to the CGI/1.1
	 * standard, so we already filled our environment with we have to have
	 * there and we won't fail anymore if it won't work out. */

	str = get_opt_str("protocol.http.user_agent");
	if (*str && strcmp(str, " ")) {
		unsigned char *ustr, ts[64] = "";

		if (!list_empty(terminals)) {
			unsigned int tslen = 0;
			struct terminal *term = terminals.prev;

			ulongcat(ts, &tslen, term->width, 3, 0);
			ts[tslen++] = 'x';
			ulongcat(ts, &tslen, term->height, 3, 0);
		}
		ustr = subst_user_agent(str, VERSION_STRING, system_name, ts);

		if (ustr) {
			env_set("HTTP_USER_AGENT", ustr, -1);
			mem_free(ustr);
		}
	}

	switch (get_opt_int("protocol.http.referer.policy")) {
	case REFERER_NONE:
		/* oh well */
		break;

	case REFERER_FAKE:
		str = get_opt_str("protocol.http.referer.fake");
		env_set("HTTP_REFERER", str, -1);
		break;

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_http_string() ? --pasky */
		if (conn->referrer)
			env_set("HTTP_REFERER", struri(conn->referrer), -1);
		break;

	case REFERER_SAME_URL:
		str = get_uri_string(conn->uri, URI_HTTP_REFERRER);
		if (str) {
			env_set("HTTP_REFERER", str, -1);
			mem_free(str);
		}
		break;
	}

	/* Protection against vim cindent bugs ;-). */
	env_set("HTTP_ACCEPT", "*/" "*", -1);

	/* We do not set HTTP_ACCEPT_ENCODING. Yeah, let's let the CGI script
	 * gzip the stuff so that the CPU doesn't at least sit idle. */

	str = get_opt_str("protocol.http.accept_language");
	if (*str) {
		env_set("HTTP_ACCEPT_LANGUAGE", str, -1);
	}
#ifdef CONFIG_NLS
	else if (get_opt_bool("protocol.http.accept_ui_language")) {
		env_set("HTTP_ACCEPT_LANGUAGE",
			language_to_iso639(current_language), -1);
	}
#endif

	if (conn->cached && !conn->cached->incomplete && conn->cached->head
	    && conn->cached->last_modified
	    && conn->cache_mode <= CACHE_MODE_CHECK_IF_MODIFIED) {
		env_set("HTTP_IF_MODIFIED_SINCE", conn->cached->last_modified, -1);
	}

	if (conn->cache_mode >= CACHE_MODE_FORCE_RELOAD) {
		env_set("HTTP_PRAGMA", "no-cache", -1);
		env_set("HTTP_CACHE_CONTROL", "no-cache", -1);
	}

	/* TODO: HTTP auth support. On the other side, it was weird over CGI
	 * IIRC. --pasky */

#ifdef CONFIG_COOKIES
	{
		struct string *cookies = send_cookies(conn->uri);

		if (cookies) {
			env_set("HTTP_COOKIE", cookies->source, -1);

			done_string(cookies);
		}
	}
#endif

	return 0;
}

static int
test_path(unsigned char *path)
{
	unsigned char *cgi_path = get_opt_str("protocol.file.cgi.path");
	unsigned char **path_ptr;
	unsigned char *filename;

	for (path_ptr = &cgi_path;
	     (filename = get_next_path_filename(path_ptr, ':'));
	     ) {
		int filelen = strlen(filename);
		int res;

		if (filename[filelen - 1] != '/') {
			add_to_strn(&filename, "/");
			filelen++;
		}

		res = strncmp(path, filename, filelen);
		mem_free(filename);
		if (!res) return 0;
	}
	return 1;
}

int
execute_cgi(struct connection *conn)
{
	unsigned char *last_slash;
	unsigned char *script;
	struct stat buf;
	pid_t pid;
	struct connection_state state = connection_state(S_OK);
	int pipe_read[2], pipe_write[2];

	if (!get_opt_bool("protocol.file.cgi.policy")) return 1;

	/* Not file referrer */
	if (conn->referrer && conn->referrer->protocol != PROTOCOL_FILE) {
		return 1;
	}

	script = get_uri_string(conn->uri, URI_PATH);
	if (!script) {
		state = connection_state(S_OUT_OF_MEM);
		goto end2;
	}
	decode_uri(script);

	if (stat(script, &buf) || !(S_ISREG(buf.st_mode))
		|| !(buf.st_mode & S_IXUSR)) {
		mem_free(script);
		return 1;
	}

	last_slash = strrchr(script, '/');
	if (last_slash++) {
		unsigned char storage;
		int res;

		/* We want to compare against path with the trailing slash. */
		storage = *last_slash;
		*last_slash = 0;
		res = test_path(script);
		*last_slash = storage;
		if (res) {
			mem_free(script);
			return 1;
		}
	} else {
		mem_free(script);
		return 1;
	}

	if (c_pipe(pipe_read) || c_pipe(pipe_write)) {
		state = connection_state_for_errno(errno);
		goto end1;
	}

	pid = fork();
	if (pid < 0) {
		state = connection_state_for_errno(errno);
		goto end0;
	}
	if (!pid) { /* CGI script */
		if (set_vars(conn, script)) {
			_exit(1);
		}
		if ((dup2(pipe_write[0], STDIN_FILENO) < 0)
			|| (dup2(pipe_read[1], STDOUT_FILENO) < 0)) {
			_exit(2);
		}
		/* We implicitly chain stderr to ELinks' stderr. */
		close_all_non_term_fd();

		last_slash[-1] = 0; set_cwd(script); last_slash[-1] = '/';
		if (execl(script, script, (char *) NULL)) {
			_exit(3);
		}

	} else { /* ELinks */
		mem_free(script);

		if (!init_http_connection_info(conn, 1, 0, 1)) {
			close(pipe_read[0]); close(pipe_read[1]);
			close(pipe_write[0]); close(pipe_write[1]);
			return 0;
		}

		close(pipe_read[1]); close(pipe_write[0]);
		conn->socket->fd = pipe_read[0];

		/* Use data socket for passing the pipe. It will be cleaned up in
	 	* close_pipe_and_read(). */
		conn->data_socket->fd = pipe_write[1];
		conn->cgi = 1;
		set_nonblocking_fd(conn->socket->fd);
		set_nonblocking_fd(conn->data_socket->fd);

		send_request(conn);
		return 0;
	}

end0:
	close(pipe_read[0]); close(pipe_read[1]);
	close(pipe_write[0]); close(pipe_write[1]);
end1:
	mem_free(script);
end2:
	abort_connection(conn, state);
	return 0;
}
