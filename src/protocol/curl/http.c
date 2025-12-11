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
#ifdef CONFIG_COOKIES
#include "cookies/cookies.h"
#endif
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
#include "osdep/sysname.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/curl/ftpes.h"
#include "protocol/curl/gopher.h"
#include "protocol/curl/http.h"
#include "protocol/curl/sftp.h"
#include "protocol/header.h"
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
#ifdef CONFIG_LIBUV
	struct curl_context *global;
#else
	GlobalInfo *global;
#endif
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
	ELOG
	http_got_data(stream, buffer, size * nmemb);
	return nmemb;
}

static size_t
my_fwrite_header(void *buffer, size_t size, size_t nmemb, void *stream)
{
	ELOG
	http_curl_got_header(stream, buffer, size * nmemb);
	return nmemb;
}

static size_t
read_post_data(void *buffer, size_t size, size_t nmemb, void *stream)
{
	ELOG
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
	ELOG
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;

	if (!http || !http->easy) {
		return;
	}
	if (!program.terminate) {
		curl_multi_remove_handle(g.multi, http->easy);
		curl_easy_cleanup(http->easy);
	}
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

static int
xferinfo_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	struct http_post *post = (struct http_post *)clientp;

	post->uploaded = ulnow;
	/* use the values */
	return 0; /* all is good */
}

static void
do_http(struct connection *conn)
{
	ELOG
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)mem_calloc(1, sizeof(*http));
	struct auth_entry *auth = find_auth(conn->uri);

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
		curl_off_t offset = conn->progress->start > 0 ? conn->progress->start : 0;
		conn->progress->seek = conn->from = offset;
		char *optstr;
		int no_verify = get_blacklist_flags(conn->uri) & SERVER_BLACKLIST_NO_CERT_VERIFY;
		struct string *cookies;
		struct string referer;
		char *bundle = getenv("CURL_CA_BUNDLE");

		http->easy = curl;
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_fwrite);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, my_fwrite_header);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, conn);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, conn);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, http->error);
		curl_easy_setopt(curl, CURLOPT_PRIVATE, conn);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
		if (!no_verify) {
			curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
		}
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, offset);
		curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)get_opt_long("protocol.http.curl_max_recv_speed", NULL));
		curl_easy_setopt(curl, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t)get_opt_long("protocol.http.curl_max_send_speed", NULL));

		char *inter_face = get_cmd_opt_str("bind-address");
		if (inter_face && *inter_face) {
			curl_easy_setopt(curl, CURLOPT_INTERFACE, inter_face);
		}

		if (conn->uri->protocol == PROTOCOL_HTTPS) {
			char *ciphers = get_opt_str("protocol.https.curl_tls13_ciphers", NULL);

			if (ciphers && *ciphers) {
				curl_easy_setopt(curl, CURLOPT_TLS13_CIPHERS, ciphers);
			}
		}

		if (bundle) {
			curl_easy_setopt(curl, CURLOPT_CAINFO, bundle);
		}
#ifdef CONFIG_COOKIES
		cookies = send_cookies(conn->uri);

		if (cookies) {
			curl_easy_setopt(curl, CURLOPT_COOKIE, cookies->source);
			done_string(cookies);
		}
#endif
		switch (get_opt_int("protocol.http.referer.policy", NULL)) {
			case REFERER_NONE:
				/* oh well */
				break;

			case REFERER_FAKE:
				optstr = get_opt_str("protocol.http.referer.fake", NULL);
				if (!optstr[0]) break;

				curl_easy_setopt(curl, CURLOPT_REFERER, optstr);
				break;

			case REFERER_TRUE:
				if (!conn->referrer) break;

				if (!init_string(&referer)) {
					break;
				}
				add_url_to_http_string(&referer, conn->referrer, URI_HTTP_REFERRER);
				curl_easy_setopt(curl, CURLOPT_REFERER, referer.source);
				done_string(&referer);
				break;

			case REFERER_SAME_URL:
				if (!init_string(&referer)) {
					break;
				}
				add_url_to_http_string(&referer, conn->uri, URI_HTTP_REFERRER);
				curl_easy_setopt(curl, CURLOPT_REFERER, referer.source);
				done_string(&referer);
				break;
		}

		if (auth) {
			curl_easy_setopt(curl, CURLOPT_USERNAME, auth->user);
			curl_easy_setopt(curl, CURLOPT_PASSWORD, auth->password);
		}

		optstr = get_opt_str("protocol.http.accept_language", NULL);
		if (optstr[0]) {
			struct string acclang;
			if (init_string(&acclang)) {
				add_to_string(&acclang, "Accept-Language: ");
				add_to_string(&acclang, optstr);
				http->list = curl_slist_append(http->list, acclang.source);
				done_string(&acclang);
			}
		}
#ifdef CONFIG_NLS
		else if (get_opt_bool("protocol.http.accept_ui_language", NULL)) {
			const char *code = language_to_iso639(current_language);

			if (code) {
				struct string acclang;
				if (init_string(&acclang)) {
					add_to_string(&acclang, "Accept-Language: ");
					add_to_string(&acclang, code);
					http->list = curl_slist_append(http->list, acclang.source);
					done_string(&acclang);
				}
			}
		}
#endif
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

		if (http->list) {
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http->list);
		}

		if (http->is_post) {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, http->post.total_upload_length);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &http->post);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_callback);
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
	ELOG
	struct connection *conn = (struct connection *)stream;
	char *buffer = (char *)buf;
	struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (len > 0) {
		add_bytes_to_string(&http->headers, buffer, len);
	}

	if (len == 2 && buffer[0] == 13 && buffer[1] == 10) {
		curl_easy_getinfo(http->easy, CURLINFO_RESPONSE_CODE, &http->code);

		if (http->code == 0L) {
			goto next;
		}

		if (!conn->cached && http->code != 103L) {
			conn->cached = get_cache_entry(conn->uri);

			if (!conn->cached) {
				abort_connection(conn, connection_state(S_OUT_OF_MEM));
				return;
			}
		}
		curl_easy_getinfo(http->easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &conn->est_length);

		if (conn->est_length != -1 && conn->progress->start > 0) {
			conn->est_length += conn->progress->start;
		}

		if (http->code != 103L) {
			mem_free_set(&conn->cached->head, memacpy(http->headers.source, http->headers.length));
			mem_free_set(&conn->cached->content_type, NULL);
		}
next:
#ifdef CONFIG_COOKIES
		if (conn->cached && conn->cached->head) {
			char *ch = conn->cached->head;
			char *cookie;

			while ((cookie = parse_header(ch, "Set-Cookie", &ch))) {
				set_cookie(conn->uri, cookie);
				mem_free(cookie);
			}
		}
#endif
		done_string(&http->headers);
		init_string(&http->headers);
	}
}

static void
http_got_data(void *stream, void *buf, size_t len)
{
	ELOG
	struct connection *conn = (struct connection *)stream;
	char *buffer = (char *)buf;

	if (len < 0) {
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

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

static char *
http_curl_check_redirect(struct connection *conn)
{
	ELOG
	struct http_curl_connection_info *http;
	if (!conn || !conn->info) {
		return NULL;
	}
	http = (struct http_curl_connection_info *)conn->info;

	if (!http->easy) {
		return NULL;
	}

	if (http->code == 201L || http->code == 301L || http->code == 302L || http->code == 303L || http->code == 307L || http->code == 308L) {
		char *url = NULL;

		curl_easy_getinfo(http->easy, CURLINFO_REDIRECT_URL, &url);

		return url;
	}
	return NULL;
}

void
http_curl_handle_error(struct connection *conn, CURLcode res)
{
	ELOG
	if (res == CURLE_OK) {
		char *url = http_curl_check_redirect(conn);
		struct http_curl_connection_info *http = (struct http_curl_connection_info *)conn->info;

		if (url) {
			(void)redirect_cache(conn->cached, url, (http->code == 303L), -1);
		} else if (http->code == 401L) {
			add_auth_entry(conn->uri, "HTTP Auth", NULL, NULL, 0);
		}
		abort_connection(conn, connection_state(S_OK));
	} else {
		abort_connection(conn, connection_state(S_CURL_ERROR - res));
	}
}

void
http_curl_protocol_handler(struct connection *conn)
{
	ELOG
	if (g.multi) {
		do_http(conn);
	}
}

#ifdef CONFIG_LIBUV
void check_multi_info(struct datauv *g)
{
	ELOG
	CURLMsg *msg;
	int pending;
	struct connection *conn;
	CURL *easy;
	CURLcode res;

	while ((msg = curl_multi_info_read(g->multi, &pending))) {
		if (msg->msg == CURLMSG_DONE) {
			easy = msg->easy_handle;
			res = msg->data.result;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);

			if (conn->uri->protocol == PROTOCOL_HTTP || conn->uri->protocol == PROTOCOL_HTTPS) {
				http_curl_handle_error(conn, res);
				continue;
			}

#ifdef CONFIG_FTP
			if (conn->uri->protocol == PROTOCOL_FTP || conn->uri->protocol == PROTOCOL_FTPES) {
				ftp_curl_handle_error(conn, res);
				continue;
			}
#endif

#ifdef CONFIG_GOPHER
			if (conn->uri->protocol == PROTOCOL_GOPHER) {
				gophers_curl_handle_error(conn, res);
				continue;
			}
#endif

#ifdef CONFIG_SFTP
			if (conn->uri->protocol == PROTOCOL_SFTP) {
				ftp_curl_handle_error(conn, res);
				continue;
			}
#endif
			else {
				abort_connection(conn, connection_state(S_OK));
			}
		}
	}
	check_bottom_halves();
}

#else
/* Check for completed transfers, and remove their easy handles */
void
check_multi_info(GlobalInfo *g)
{
	ELOG
	//char *eff_url;
	CURLMsg *msg;
	int msgs_left;
	struct connection *conn;
	CURL *easy;
	CURLcode res;

	//fprintf(stderr, "REMAINING: %d\n", g->still_running);

	while ((msg = curl_multi_info_read(g->multi, &msgs_left))) {
		if (msg->msg == CURLMSG_DONE) {
			easy = msg->easy_handle;
			res = msg->data.result;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);

			if (conn->uri->protocol == PROTOCOL_HTTP || conn->uri->protocol == PROTOCOL_HTTPS) {
				http_curl_handle_error(conn, res);
				continue;
			}

#ifdef CONFIG_FTP
			if (conn->uri->protocol == PROTOCOL_FTP || conn->uri->protocol == PROTOCOL_FTPES) {
				ftp_curl_handle_error(conn, res);
				continue;
			}
#endif

#ifdef CONFIG_GOPHER
			if (conn->uri->protocol == PROTOCOL_GOPHER) {
				gophers_curl_handle_error(conn, res);
				continue;
			}
#endif

#ifdef CONFIG_SFTP
			if (conn->uri->protocol == PROTOCOL_SFTP) {
				ftp_curl_handle_error(conn, res);
				continue;
			}
#endif
			else {
				abort_connection(conn, connection_state(S_OK));
			}
		}
	}
	check_bottom_halves();
#if 0
	if (g->still_running == 0 && g->stopped) {
		event_base_loopbreak(g->evbase);
	}
#endif
}
#endif
