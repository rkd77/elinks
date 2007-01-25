/* Internal "cgi" protocol implementation */
/* $Id: cgi.c,v 1.85.2.2 2005/04/05 21:08:42 jonas Exp $ */

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
#include "lowlevel/sysname.h"
#include "mime/backend/common.h"
#include "osdep/osdep.h"
#include "protocol/file/cgi.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/string.h"


static void
close_pipe_and_read(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) return;

	memcpy(rb->data, "HTTP/1.0 200 OK\r\n", 17);
	rb->len = 17;
	rb->freespace -= 17;
	rb->close = READ_BUFFER_END_ONCLOSE;
	conn->unrestartable = 1;
	close(conn->cgi_pipes[1]);
	conn->data_socket.fd = conn->cgi_pipes[1] = -1;
	set_connection_state(conn, S_SENT);
	set_connection_timeout(conn);
	read_from_socket(conn, &conn->socket, rb, http_got_header);
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
		abort_conn_with_state(conn, S_OUT_OF_MEM);
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

	set_connection_timeout(conn);

	/* Use data socket for passing the pipe. It will be cleaned up in
	 * close_pipe_and_read(). */
	conn->data_socket.fd = conn->cgi_pipes[1];

	write_to_socket(conn, &conn->data_socket, data.source, data.length, close_pipe_and_read);

	done_string(&data);
	set_connection_state(conn, S_SENT);
#undef POST_BUFFER_SIZE
}

static void
send_request(struct connection *conn)
{
	if (conn->uri->post) send_post_data(conn);
	else close_pipe_and_read(conn);
}

/* This function sets CGI environment variables. */
static int
set_vars(struct connection *conn, unsigned char *script)
{
	unsigned char *post = conn->uri->post;
	unsigned char *query = get_uri_string(conn->uri, URI_QUERY);
	unsigned char *str;
	int res = setenv("QUERY_STRING", empty_string_or_(query), 1);

	mem_free_if(query);
	if (res) return -1;

	if (post) {
		unsigned char *postend = strchr(post, '\n');
		unsigned char buf[16];

		if (postend) {
			*postend = '\0';
			res = setenv("CONTENT_TYPE", post, 1);
			*postend = '\n';
			if (res) return -1;
			post = postend + 1;
		}
		snprintf(buf, 16, "%d", (int) strlen(post) / 2);
		if (setenv("CONTENT_LENGTH", buf, 1)) return -1;
	}

	if (setenv("REQUEST_METHOD", post ? "POST" : "GET", 1)) return -1;
	if (setenv("SERVER_SOFTWARE", "ELinks/" VERSION, 1)) return -1;
	if (setenv("SERVER_PROTOCOL", "HTTP/1.0", 1)) return -1;
	/* XXX: Maybe it is better to set this to an empty string? --pasky */
	if (setenv("SERVER_NAME", "localhost", 1)) return -1;
	/* XXX: Maybe it is better to set this to an empty string? --pasky */
	if (setenv("REMOTE_ADDR", "127.0.0.1", 1)) return -1;
	if (setenv("GATEWAY_INTERFACE", "CGI/1.1", 1)) return -1;
	/* This is the path name extracted from the URI and decoded, per
	 * http://cgi-spec.golux.com/draft-coar-cgi-v11-03-clean.html#8.1 */
	if (setenv("SCRIPT_NAME", script, 1)) return -1;
	if (setenv("SCRIPT_FILENAME", script, 1)) return -1;
	if (setenv("PATH_TRANSLATED", script, 1)) return -1;

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
			setenv("HTTP_USER_AGENT", ustr, 1);
			mem_free(ustr);
		}
	}

	switch (get_opt_int("protocol.http.referer.policy")) {
	case REFERER_NONE:
		/* oh well */
		break;

	case REFERER_FAKE:
		str = get_opt_str("protocol.http.referer.fake");
		setenv("HTTP_REFERER", str, 1);
		break;

	case REFERER_TRUE:
		/* XXX: Encode as in add_url_to_http_string() ? --pasky */
		if (conn->referrer)
			setenv("HTTP_REFERER", struri(conn->referrer), 1);
		break;

	case REFERER_SAME_URL:
		str = get_uri_string(conn->uri, URI_HTTP_REFERRER);
		if (str) {
			setenv("HTTP_REFERER", str, 1);
			mem_free(str);
		}
		break;
	}

	/* Protection against vim cindent bugs ;-). */
	setenv("HTTP_ACCEPT", "*/" "*", 1);

	/* We do not set HTTP_ACCEPT_ENCODING. Yeah, let's let the CGI script
	 * gzip the stuff so that the CPU doesn't at least sit idle. */

	str = get_opt_str("protocol.http.accept_language");
	if (*str) {
		setenv("HTTP_ACCEPT_LANGUAGE", str, 1);
	}
#ifdef ENABLE_NLS
	else if (get_opt_bool("protocol.http.accept_ui_language")) {
		setenv("HTTP_ACCEPT_LANGUAGE",
			language_to_iso639(current_language), 1);
	}
#endif

	if (conn->cached && !conn->cached->incomplete && conn->cached->head
	    && conn->cached->last_modified
	    && conn->cache_mode <= CACHE_MODE_CHECK_IF_MODIFIED) {
		setenv("HTTP_IF_MODIFIED_SINCE", conn->cached->last_modified, 1);
	}

	if (conn->cache_mode >= CACHE_MODE_FORCE_RELOAD) {
		setenv("HTTP_PRAGMA", "no-cache", 1);
		setenv("HTTP_CACHE_CONTROL", "no-cache", 1);
	}

	/* TODO: HTTP auth support. On the other side, it was weird over CGI
	 * IIRC. --pasky */

#ifdef CONFIG_COOKIES
	{
		struct string *cookies = send_cookies(conn->uri);

		if (cookies) {
			setenv("HTTP_COOKIE", cookies->source, 1);

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
	int scriptlen;
	struct stat buf;
	pid_t pid;
	enum connection_state state = S_OK;
	int pipe_read[2], pipe_write[2];

	if (!get_opt_bool("protocol.file.cgi.policy")) return 1;

	/* Not file referrer */
	if (conn->referrer && conn->referrer->protocol != PROTOCOL_FILE) {
		return 1;
	}

	script = get_uri_string(conn->uri, URI_PATH);
	if (!script) {
		state = S_OUT_OF_MEM;
		goto end2;
	}
	decode_uri(script);
	scriptlen = strlen(script);

	if (stat(script, &buf) || !(S_ISREG(buf.st_mode))
		|| !(buf.st_mode & S_IEXEC)) {
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
		state = -errno;
		goto end1;
	}

	pid = fork();
	if (pid < 0) {
		state = -errno;
		goto end0;
	}
	if (!pid) { /* CGI script */
		int i;

		if (set_vars(conn, script)) {
			_exit(1);
		}
		if ((dup2(pipe_write[0], STDIN_FILENO) < 0)
			|| (dup2(pipe_read[1], STDOUT_FILENO) < 0)) {
			_exit(2);
		}
		/* We implicitly chain stderr to ELinks' stderr. */
		for (i = 3; i < 1024; i++) {
			close(i);
		}

		last_slash[-1] = 0; set_cwd(script); last_slash[-1] = '/';
		if (execl(script, last_slash, NULL)) {
			_exit(3);
		}

	} else { /* ELinks */
		struct http_connection_info *info;

		info = mem_calloc(1, sizeof(*info));
		if (!info) {
			state = S_OUT_OF_MEM;
			goto end0;
		}
		mem_free(script);
		conn->info = info;
		info->sent_version.major = 1;
		info->sent_version.minor = 0;
		info->close = 1;

		close(pipe_read[1]); close(pipe_write[0]);
		conn->cgi_pipes[0] = pipe_read[0];
		conn->cgi_pipes[1] = pipe_write[1];
		set_nonblocking_fd(conn->cgi_pipes[0]);
		set_nonblocking_fd(conn->cgi_pipes[1]);
		conn->socket.fd = conn->cgi_pipes[0];

		send_request(conn);
		return 0;
	}

end0:
	close(pipe_read[0]); close(pipe_read[1]);
	close(pipe_write[0]); close(pipe_write[1]);
end1:
	mem_free(script);
end2:
	abort_conn_with_state(conn, state);
	return 0;
}
