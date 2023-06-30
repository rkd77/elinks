/* curl http protocol implementation */

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
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/stat.h"
#include "osdep/sysname.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/curl/http.h"
#include "protocol/http/http.h"
#include "protocol/http/post.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* Types and structs */

struct http_curl_connection_info {
	CURL *easy;
	struct curl_slist *list;
	char *url;
	char *post_buffer;
	struct http_post post;
	struct string post_headers;
	struct string headers;
	GlobalInfo *global;
	char error[CURL_ERROR_SIZE];
	int conn_state;
	int buf_pos;
	long code;
	unsigned int is_post:1;
};

static void http_got_data(void *stream, void *buffer, size_t len);
static void http_curl_got_header(void *stream, void *buffer, size_t len);

static size_t
my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
	http_got_data(stream, buffer, size * nmemb);
	return nmemb;
}

static size_t
my_fwrite_header(void *buffer, size_t size, size_t nmemb, void *stream)
{
	http_curl_got_header(stream, buffer, size * nmemb);
	return nmemb;
}

static size_t
read_post_data(void *buffer, size_t size, size_t nmemb, void *stream)
{
	struct connection *conn = (struct connection *)stream;
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;
	struct connection_state error;
	int max = (int)(size * nmemb);
	int ret = read_http_post(&http->post, (char *)buffer, max, &error);

	return (size_t)ret;
}

static void
done_http_curl(struct connection *conn)
{
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;

	if (!http || !http->easy) {
		return;
	}
	curl_multi_remove_handle(g.multi, http->easy);
	curl_easy_cleanup(http->easy);
	done_string(&http->headers);
	done_string(&http->post_headers);
	mem_free_if(http->post_buffer);

	if (http->is_post) {
		done_http_post(&http->post);
	}

	if (http->list) {
		curl_slist_free_all(http->list);
	}
}

static void
do_http(struct connection *conn)
{
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)mem_calloc(1, sizeof(*http));
	struct string u;
	CURL *curl;

	if (!http) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	if (!init_string(&http->headers)) {
		mem_free(http);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	if (!init_string(&http->post_headers)) {
		done_string(&http->headers);
		mem_free(http);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	conn->info = http;
	conn->done = done_http_curl;

	conn->from = 0;
	conn->unrestartable = 1;

	char *url = get_uri_string(conn->uri, URI_HOST | URI_PORT | URI_DATA);

	if (!url) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	if (!init_string(&u)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (conn->uri->protocol == PROTOCOL_HTTP) {
		add_to_string(&u, "http://");
	} else {
		add_to_string(&u, "https://");
	}
	add_to_string(&u, url);
	mem_free(url);
	curl = curl_easy_init();


	if (curl) {
		CURLMcode rc;
		char *optstr;
		int no_verify = get_blacklist_flags(conn->uri) & SERVER_BLACKLIST_NO_CERT_VERIFY;

		http->easy = curl;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, my_fwrite_header);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, conn);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, conn);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, http->error);
		curl_easy_setopt(curl, CURLOPT_PRIVATE, conn);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, my_fwrite_header);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

		if (conn->uri->post) {
			char *postend = strchr(conn->uri->post, '\n');
			char *post = postend ? postend + 1 : conn->uri->post;
			char *file = strchr(post, FILE_CHAR);

			if (!file) {
				size_t length = strlen(post);
				size_t size = length / 2;
				size_t total = 0;

				http->post_buffer = mem_alloc(size + 1);

				if (http->post_buffer) {
					http->post_buffer[size] = '\0';

					while (total < size) {
						int h1, h2;

						h1 = unhx(post[0]);
						assertm(h1 >= 0 && h1 < 16, "h1 in the POST buffer is %d (%d/%c)", h1, post[0], post[0]);
						if_assert_failed h1 = 0;

						h2 = unhx(post[1]);
						assertm(h2 >= 0 && h2 < 16, "h2 in the POST buffer is %d (%d/%c)", h2, post[1], post[1]);
						if_assert_failed h2 = 0;

						http->post_buffer[total++] = (h1<<4) + h2;
						post += 2;
					}
				}
			} else {
				struct connection_state error;
				http->is_post = 1;
				init_http_post(&http->post);

				if (postend) {
					add_to_string(&http->post_headers, "Content-Type: ");
					add_bytes_to_string(&http->post_headers, conn->uri->post, postend - conn->uri->post);
					http->list = curl_slist_append(http->list, http->post_headers.source);
				}

				if (!open_http_post(&http->post, post, &error)) {
					abort_connection(conn, connection_state(S_OUT_OF_MEM));
					return;
				}
			}
		}

		//fprintf(stderr, "Adding easy %p to multi %p (%s)\n", curl, g.multi, u.source);
		set_connection_state(conn, connection_state(S_TRANS));
		curl_easy_setopt(curl, CURLOPT_URL, u.source);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
		curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_3);

		if (http->is_post) {
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http->list);
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, http->post.total_upload_length);
			curl_easy_setopt(curl, CURLOPT_READDATA, conn);
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_post_data);
		} else if (http->post_buffer) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, http->post_buffer);
		}

		optstr = get_opt_str("protocol.http.user_agent", NULL);

		if (*optstr && strcmp(optstr, " ")) {
			char *ustr, ts[64] = "";
			/* TODO: Somehow get the terminal in which the
			 * document will actually be displayed.  */
			struct terminal *term = get_default_terminal();

			if (term) {
				unsigned int tslen = 0;

				ulongcat(ts, &tslen, term->width, 3, 0);
				ts[tslen++] = 'x';
				ulongcat(ts, &tslen, term->height, 3, 0);
			}
			ustr = subst_user_agent(optstr, VERSION_STRING, system_name, ts);

			if (ustr) {
				curl_easy_setopt(curl, CURLOPT_USERAGENT, ustr);
				mem_free(ustr);
			}
		}

		if (no_verify) {
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		}

		rc = curl_multi_add_handle(g.multi, curl);
		mcode_or_die("new_conn: curl_multi_add_handle", rc);
	}
	done_string(&u);
}

static void
http_curl_got_header(void *stream, void *buf, size_t len)
{
	struct connection *conn = (struct connection *)stream;
	char *buffer = (char *)buf;
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;

	/* XXX: This probably belongs rather to connect.c ? */
	set_connection_timeout(conn);

	if (!conn->cached) conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (len == 2 && buffer[0] == 13 && buffer[1] == 10) {
		curl_easy_getinfo(http->easy, CURLINFO_RESPONSE_CODE, &http->code);
	}

	if (len > 0) {
		if (!strncmp(buffer, "HTTP/", 5)) {
			done_string(&http->headers);
			init_string(&http->headers);
		}
		add_bytes_to_string(&http->headers, buffer, len);
		return;
	}
}

static void
http_got_data(void *stream, void *buf, size_t len)
{
	struct connection *conn = (struct connection *)stream;
	char *buffer = (char *)buf;
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;

	/* XXX: This probably belongs rather to connect.c ? */
	set_connection_timeout(conn);

	if (!conn->cached) conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (len > 0) {
		if (conn->from == 0) {
			mem_free_set(&conn->cached->head, memacpy(http->headers.source, http->headers.length));
			mem_free_set(&conn->cached->content_type, NULL);
		}
		if (add_fragment(conn->cached, conn->from, buffer, len) == 1) {
			conn->tries = 0;
		}
		if (conn->from == 0 && conn->est_length == -1) {
			curl_easy_getinfo(http->easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &conn->est_length);
		}
		conn->from += len;
		conn->received += len;
		return;
	}
	abort_connection(conn, connection_state(S_OK));
}

static char *
http_curl_check_redirect(struct connection *conn)
{
	struct http_curl_connection_info *http;
	if (!conn || !conn->info) {
		return NULL;
	}
	http = (struct http_curl_connection_info *)conn->info;

	if (!http->easy) {
		return NULL;
	}

	if (http->code == 301L || http->code == 302L) {
		char *url = NULL;

		curl_easy_getinfo(http->easy, CURLINFO_REDIRECT_URL, &url);

		return url;
	}
	return NULL;
}

void
http_curl_handle_error(struct connection *conn, CURLcode res)
{
	if (res == CURLE_OK) {
		char *url = http_curl_check_redirect(conn);

		if (url) {
			redirect_cache(conn->cached, url, 0, 0);
			abort_connection(conn, connection_state(S_OK));
			return;
		}
		abort_connection(conn, connection_state(S_OK));
		return;
	} else {
		abort_connection(conn, connection_state(S_CURL_ERROR - res));
	}
}

void
http_curl_protocol_handler(struct connection *conn)
{
	if (g.multi) {
		do_http(conn);
	}
}
