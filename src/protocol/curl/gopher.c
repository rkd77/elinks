/* curl gopher protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>	/* For converting permissions to strings */
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <curl/curl.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "main/main.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/stat.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/curl/gopher.h"
#include "protocol/curl/http.h"
#include "protocol/gopher/gopher.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef CONFIG_GOPHER

/* Types and structs */

struct gophers_connection_info {
	CURL *easy;
	char *url;
#ifdef CONFIG_LIBUV
	struct datauv *global;
#else
	GlobalInfo *global;
#endif
	char error[CURL_ERROR_SIZE];
	int conn_state;
	int buf_pos;
	size_t info_number;

	unsigned int dir:1;          /* Directory listing in progress */
};

static void gophers_got_data(void *stream, void *buffer, size_t len);

static size_t
my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
	ELOG
	gophers_got_data(stream, buffer, size * nmemb);
	return nmemb;
}

static void
done_gophers(struct connection *conn)
{
	ELOG
	struct gophers_connection_info *gopher = (struct gophers_connection_info *)conn->info;

	if (!gopher || !gopher->easy) {
		return;
	}

	if (!program.terminate) {
		curl_multi_remove_handle(g.multi, gopher->easy);
		curl_easy_cleanup(gopher->easy);
	}
}

static void
do_gophers(struct connection *conn)
{
	ELOG
	struct gophers_connection_info *gopher = (struct gophers_connection_info *)mem_calloc(1, sizeof(*gopher));
	struct auth_entry *auth = find_auth(conn->uri);
	char *url;
	struct string u;
	CURL *curl;

	if (!gopher) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	conn->info = gopher;
	conn->done = done_gophers;

	char entity = '1';
	char *selector = conn->uri->data;
	int selectorlen = conn->uri->datalen;

	/* Get entity type, and selector string. */
	/* Pick up gopher_entity */
	if (selectorlen > 1 && selector[1] == '/') {
		entity = *selector++;
		selectorlen--;
	}
	gopher->dir = (entity == '1');

	url = get_uri_string(conn->uri, URI_HOST | URI_PORT | URI_DATA);

	if (!url) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	if (!init_string(&u)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (conn->uri->protocol == PROTOCOL_GOPHER) {
		add_to_string(&u, "gopher://");
	} else {
		add_to_string(&u, "gophers://");
	}

	if (0 && auth) {
		add_to_string(&u, auth->user);
		add_char_to_string(&u, ':');
		add_to_string(&u, auth->password);
		add_char_to_string(&u, '@');
	}
	add_to_string(&u, url);
	mem_free(url);
	curl = curl_easy_init();

	if (curl) {
		CURLMcode rc;
		curl_off_t offset = conn->progress->start > 0 ? conn->progress->start : 0;
		conn->progress->seek = conn->from = offset;
		gopher->easy = curl;
		/* Define our callback to get called when there's data to be written */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite);
		/* Set a pointer to our struct to pass to the callback */
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, conn);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, gopher->error);
		curl_easy_setopt(curl, CURLOPT_PRIVATE, conn);
		curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, offset);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		char *inter_face = get_cmd_opt_str("bind-address");
		if (inter_face && *inter_face) {
			curl_easy_setopt(curl, CURLOPT_INTERFACE, inter_face);
		}
		//curl_easy_setopt(curl, CURLOPT_XFERINFODATA, conn);
		//curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

		/* enable TCP keep-alive for this transfer */
//		curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
		/* keep-alive idle time to 120 seconds */
//		curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
		/* interval time between keep-alive probes: 60 seconds */
//		curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);

		/* We activate SSL and we require it for control */
		if (0) {
			char *bundle = getenv("CURL_CA_BUNDLE");
			char *ciphers = get_opt_str("protocol.gopher.curl_tls13_ciphers", NULL);

			if (bundle) {
				curl_easy_setopt(curl, CURLOPT_CAINFO, bundle);
			}
			if (ciphers && *ciphers) {
				curl_easy_setopt(curl, CURLOPT_TLS13_CIPHERS, ciphers);
			}
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_CONTROL);
		}
///		curl_easy_setopt(curl, CURLOPT_STDERR, stream);
		/* Switch on full protocol/debug output */
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

		//fprintf(stderr, "Adding easy %p to multi %p (%s)\n", curl, g.multi, u.source);
		set_connection_state(conn, connection_state(S_TRANS));
		curl_easy_setopt(curl, CURLOPT_URL, u.source);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

		rc = curl_multi_add_handle(g.multi, curl);
		mcode_or_die("new_conn: curl_multi_add_handle", rc);
	}
	done_string(&u);
}

static char *
check_gopher_last_line(char *line, char *end)
{
	ELOG
	assert(line < end);

	if (line[0] == '.' && line[1] == 13 && line[2] == 10) {
		return NULL;
	}
	return line;
}

/* Parse a Gopher Menu document */
static void
read_gopher_directory_data(void *stream, void *buf, size_t len)
{
	ELOG
	struct connection *conn = (struct connection *)stream;
	struct gophers_connection_info *gopher = (struct gophers_connection_info *)conn->info;

	char *data = mem_alloc(len + 1);

	if (!data) {
		return;
	}
	memcpy(data, buf, len);
	data[len] = '\0';

	char *data2 = data;

	struct connection_state state = connection_state(S_TRANS);
	struct string buffer;
	char *end;

	if (conn->from == 0) {
		struct connection_state state = init_directory_listing(&buffer, conn->uri);
		if (!is_in_state(state, S_OK)) {
			mem_free(data);
			abort_connection(conn, state);
		}
	} else if (!init_string(&buffer)) {
		mem_free(data);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	while ((end = get_gopher_line_end(data2, len))) {
		char *line = check_gopher_last_line(data2, end);

		/* Break on line with a dot by itself */
		if (!line) {
			state = connection_state(S_OK);
			break;
		}
		add_gopher_menu_line(&buffer, line);
		int step = end - data2;
		conn->received += step;
		data2 += step;
		len -= step;
	}

	if (!is_in_state(state, S_TRANS)) {
		add_to_string(&buffer,
			"</pre>\n"
			"</body>\n"
			"</html>\n");
	}
	add_fragment(conn->cached, conn->from, buffer.source, buffer.length);
	conn->from += buffer.length;

	done_string(&buffer);
	mem_free(data);
}

static void
gophers_got_data(void *stream, void *buf, size_t len)
{
	ELOG
	struct connection *conn = (struct connection *)stream;
	char *buffer = (char *)buf;
	struct gophers_connection_info *gopher = (struct gophers_connection_info *)conn->info;

	/* XXX: This probably belongs rather to connect.c ? */
	set_connection_timeout(conn);

	if (!conn->cached) {
		conn->cached = get_cache_entry(conn->uri);

		if (!conn->cached) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
	}

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (gopher->dir) {
		read_gopher_directory_data(stream, buffer, len);
	} else {
		if (len > 0) {
			if (add_fragment(conn->cached, conn->from, buffer, len) == 1) {
				conn->tries = 0;
			}
			conn->from += len;
			conn->received += len;
			return;
		}
		abort_connection(conn, connection_state(S_OK));
	}
}

void
gophers_curl_handle_error(struct connection *conn, CURLcode res)
{
	ELOG
	if (res == CURLE_OK) {
		abort_connection(conn, connection_state(S_OK));
		return;
	}
	abort_connection(conn, connection_state(S_CURL_ERROR - res));
}

void
gophers_protocol_handler(struct connection *conn)
{
	ELOG
	if (g.multi) {
		do_gophers(conn);
	}
}
#endif
