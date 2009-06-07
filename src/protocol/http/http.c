/* Internal "http" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "cookies/cookies.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "osdep/sysname.h"
#include "protocol/auth/auth.h"
#include "protocol/auth/digest.h"
#include "protocol/date.h"
#include "protocol/header.h"
#include "protocol/http/blacklist.h"
#include "protocol/http/codes.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/base64.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef CONFIG_GSSAPI
#include "http_negotiate.h"
#endif

struct http_version {
	int major;
	int minor;
};

#define HTTP_0_9(x)	 ((x).major == 0 && (x).minor == 9)
#define HTTP_1_0(x)	 ((x).major == 1 && (x).minor == 0)
#define HTTP_1_1(x)	 ((x).major == 1 && (x).minor == 1)
#define PRE_HTTP_1_0(x)  ((x).major < 1)
#define PRE_HTTP_1_1(x)  (PRE_HTTP_1_0(x) || HTTP_1_0(x))
#define POST_HTTP_1_0(x) ((x).major > 1 || ((x).major == 1 && (x).minor > 0))
#define POST_HTTP_1_1(x) ((x).major > 1 || ((x).major == 1 && (x).minor > 1))


struct http_connection_info {
	enum blacklist_flags bl_flags;
	struct http_version recv_version;
	struct http_version sent_version;

	int close;

#define LEN_CHUNKED -2 /* == we get data in unknown number of chunks */
#define LEN_FINISHED 0
	int length;

	/* Either bytes coming in this chunk yet or "parser state". */
#define CHUNK_DATA_END	-3
#define CHUNK_ZERO_SIZE	-2
#define CHUNK_SIZE	-1
	int chunk_remaining;

	int code;
};


static struct auth_entry proxy_auth;

static unsigned char *accept_charset = NULL;


static struct option_info http_options[] = {
	INIT_OPT_TREE("protocol", N_("HTTP"),
		"http", 0,
		N_("HTTP-specific options.")),


	INIT_OPT_TREE("protocol.http", N_("Server bug workarounds"),
		"bugs", 0,
		N_("Server-side HTTP bugs workarounds.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Do not send Accept-Charset"),
		"accept_charset", 0, 1,
		N_("The Accept-Charset header is quite long and sending it "
		"can trigger bugs in some rarely found servers.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Allow blacklisting"),
		"allow_blacklist", 0, 1,
		N_("Allow blacklisting of buggy servers.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Broken 302 redirects"),
		"broken_302_redirect", 0, 1,
		N_("Broken 302 redirect (violates RFC but compatible with "
		"Netscape). This is a problem for a lot of web discussion "
		"boards and the like. If they will do strange things to you, "
		"try to play with this.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("No keepalive after POST requests"),
		"post_no_keepalive", 0, 0,
		N_("Disable keepalive connection after POST request.")),

	INIT_OPT_BOOL("protocol.http.bugs", N_("Use HTTP/1.0"),
		"http10", 0, 0,
		N_("Use HTTP/1.0 protocol instead of HTTP/1.1.")),

	INIT_OPT_TREE("protocol.http", N_("Proxy configuration"),
		"proxy", 0,
		N_("HTTP proxy configuration.")),

	INIT_OPT_STRING("protocol.http.proxy", N_("Host and port-number"),
		"host", 0, "",
		N_("Host and port-number (host:port) of the HTTP proxy, "
		"or blank. If it's blank, HTTP_PROXY environment variable "
		"is checked as well.")),

	INIT_OPT_STRING("protocol.http.proxy", N_("Username"),
		"user", 0, "",
		N_("Proxy authentication username.")),

	INIT_OPT_STRING("protocol.http.proxy", N_("Password"),
		"passwd", 0, "",
		N_("Proxy authentication password.")),


	INIT_OPT_TREE("protocol.http", N_("Referer sending"),
		"referer", 0,
		N_("HTTP referer sending options. HTTP referer is a special "
		"header sent in the HTTP requests, which is supposed to "
		"contain the previous page visited by the browser."
		"This way, the server can know what link did you follow "
		"when accessing that page. However, this behaviour can "
		"unfortunately considerably affect privacy and can lead even "
		"to a security problem on some badly designed web pages.")),

	INIT_OPT_INT("protocol.http.referer", N_("Policy"),
		"policy", 0,
		REFERER_NONE, REFERER_TRUE, REFERER_TRUE,
		N_("Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)")),

	INIT_OPT_STRING("protocol.http.referer", N_("Fake referer URL"),
		"fake", 0, "",
		N_("Fake referer to be sent when policy is 2.")),


	INIT_OPT_STRING("protocol.http", N_("Send Accept-Language header"),
		"accept_language", 0, "",
		N_("Send Accept-Language header.")),

	INIT_OPT_BOOL("protocol.http", N_("Use UI language as Accept-Language"),
		"accept_ui_language", 0, 1,
		N_("Request localised versions of documents from web-servers "
		"(using the Accept-Language header) using the language "
		"you have configured for ELinks' user-interface (this also "
		"affects navigator.language ECMAScript value available to "
		"scripts). Note that some see this as a potential security "
		"risk because it tells web-masters and the FBI sniffers "
		"about your language preference.")),

	/* http://www.eweek.com/c/a/Desktops-and-Notebooks/Intel-Psion-End-Dispute-Concerning-Netbook-Trademark-288875/
	 * responds with "Transfer-Encoding: chunked" and
	 * "Content-Encoding: gzip" but does not compress the first chunk
	 * and the last chunk, causing ELinks to display garbage.
	 * (If User-Agent includes "Gecko" (case sensitive), then
	 * that server correctly compresses the whole stream.)
	 * ELinks should instead report the decompression error (bug 1017)
	 * or perhaps even blacklist the server for compression and retry.
	 * Until that has been implemented, disable compression by default.  */
	INIT_OPT_BOOL("protocol.http", N_("Enable on-the-fly compression"),
		"compression", 0, 0,
		N_("If enabled, the capability to receive compressed content "
		"(gzip and/or bzip2) is announced to the server, which "
		"usually sends the reply compressed, thus saving some "
		"bandwidth at slight CPU expense.\n"
		"\n"
		"If ELinks displays a incomplete page or garbage, try "
		"disabling this option. If that helps, there may be a bug in "
		"the decompression part of ELinks. Please report such bugs.\n"
		"\n"
		"If ELinks has been compiled without compression support, "
		"this option has no effect. To check the supported features, "
		"see Help -> About.")),

	INIT_OPT_BOOL("protocol.http", N_("Activate HTTP TRACE debugging"),
		"trace", 0, 0,
		N_("If active, all HTTP requests are sent with TRACE as "
		"their method rather than GET or POST. This is useful for "
		"debugging of both ELinks and various server-side scripts "
		"--- the server only returns the client's request back to "
		"the client verbatim. Note that this type of request may "
		"not be enabled on all servers.")),

	/* OSNews.com is supposed to be relying on the textmode token, at least. */
	INIT_OPT_STRING("protocol.http", N_("User-agent identification"),
		"user_agent", 0, "ELinks/%v (textmode; %s; %t-%b)",
		N_("Change the User Agent ID. That means identification "
		"string, which is sent to HTTP server when a document is "
		"requested. The 'textmode' token in the first field is our "
		"silent attempt to establish this as a standard for new "
		"textmode user agents, so that the webmasters can have "
		"just a single uniform test for these if they are e.g. "
		"pushing some lite version to them automagically.\n"
		"\n"
		"Use \" \" if you don't want any User-Agent header to be sent "
		"at all.\n"
		"\n"
		"%v in the string means ELinks version,\n"
		"%s in the string means system identification,\n"
		"%t in the string means size of the terminal,\n"
		"%b in the string means number of bars displayed by ELinks.")),


	INIT_OPT_TREE("protocol", N_("HTTPS"),
  		"https", 0,
		N_("HTTPS-specific options.")),

	INIT_OPT_TREE("protocol.https", N_("Proxy configuration"),
	  	"proxy", 0,
		N_("HTTPS proxy configuration.")),

	INIT_OPT_STRING("protocol.https.proxy", N_("Host and port-number"),
	  	"host", 0, "",
		N_("Host and port-number (host:port) of the HTTPS CONNECT "
		"proxy, or blank. If it's blank, HTTPS_PROXY environment "
		"variable is checked as well.")),
	NULL_OPTION_INFO,
};

static void done_http();

struct module http_protocol_module = struct_module(
	/* name: */		N_("HTTP"),
	/* options: */		http_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_http
);


static void
done_http(void)
{
	mem_free_if(proxy_auth.realm);
	mem_free_if(proxy_auth.nonce);
	mem_free_if(proxy_auth.opaque);

	free_blacklist();

	if (accept_charset)
		mem_free(accept_charset);
}

static void
init_accept_charset(void)
{
	struct string ac;
	unsigned char *cs;
	int i;

	if (!init_string(&ac)) return;

	for (i = 0; (cs = get_cp_mime_name(i)); i++) {
		if (ac.length) {
			add_to_string(&ac, ", ");
		} else {
			add_to_string(&ac, "Accept-Charset: ");
		}
		add_to_string(&ac, cs);
	}

	if (ac.length) {
		add_crlf_to_string(&ac);
	}

	accept_charset = squeezastring(&ac);

	done_string(&ac);
}


unsigned char *
subst_user_agent(unsigned char *fmt, unsigned char *version,
		 unsigned char *sysname, unsigned char *termsize)
{
	struct string agent;

	if (!init_string(&agent)) return NULL;

	while (*fmt) {
		int p;

		for (p = 0; fmt[p] && fmt[p] != '%'; p++);

		add_bytes_to_string(&agent, fmt, p);
		fmt += p;

		if (*fmt != '%') continue;

		fmt++;
		switch (*fmt) {
			case 'b':
				if (!list_empty(sessions)) {
					unsigned char bs[4] = "";
					int blen = 0;
					struct session *ses = sessions.prev;
					int bars = ses->status.show_status_bar
						+ ses->status.show_tabs_bar
						+ ses->status.show_title_bar;

					ulongcat(bs, &blen, bars, 2, 0);
					add_to_string(&agent, bs);
				}
				break;
			case 'v':
				add_to_string(&agent, version);
				break;
			case 's':
				add_to_string(&agent, sysname);
				break;
			case 't':
				if (termsize)
					add_to_string(&agent, termsize);
				break;
			default:
				add_bytes_to_string(&agent, fmt - 1, 2);
				break;
		}
		if (*fmt) fmt++;
	}

	return agent.source;
}

static void
add_url_to_http_string(struct string *header, struct uri *uri, int components)
{
	/* This block substitues spaces in URL by %20s. This is
	 * certainly not the right place where to do it, but now the
	 * behaviour is at least improved compared to what we had
	 * before. We should probably encode all URLs as early as
	 * possible, and possibly decode them back in protocol
	 * backends. --pasky */
	unsigned char *string = get_uri_string(uri, components);
	unsigned char *data = string;

	if (!string) return;

	while (*data) {
		int len = strcspn(data, " \t\r\n\\");

		add_bytes_to_string(header, data, len);

		if (!data[len]) break;

		if (data[len++] == '\\')
			add_char_to_string(header, '/');
		else
			add_to_string(header, "%20");

		data	+= len;
	}

	mem_free(string);
}

/* Parse from @end - 1 to @start and set *@value to integer found.
 * It returns -1 if not a number, 0 otherwise.
 * @end should be > @start. */
static int
revstr2num(unsigned char *start, unsigned char *end, int *value)
{
	int q = 1, val = 0;

	do {
		--end;
		if (!isdigit(*end)) return -1; /* NaN */
		val += (*end - '0') * q;
		q *= 10;
	} while (end > start);

	*value = val;
	return 0;
}

/* This function extracts code, major and minor version from string
 * "\s*HTTP/\d+.\d+\s+\d\d\d..."
 * It returns a negative value on error, 0 on success.
 */
static int
get_http_code(struct read_buffer *rb, int *code, struct http_version *version)
{
	unsigned char *head = rb->data;
	unsigned char *start;

	*code = 0;
	version->major = 0;
	version->minor = 0;

	/* Ignore spaces. */
	while (*head == ' ') head++;

	/* HTTP/ */
	if (c_toupper(*head) != 'H' || c_toupper(*++head) != 'T' ||
	    c_toupper(*++head) != 'T' || c_toupper(*++head) != 'P'
	    || *++head != '/')
		return -1;

	/* Version */
	start = ++head;
	/* Find next '.' */
	while (*head && *head != '.') head++;
	/* Sanity check. */
	if (!*head || !(head - start)
	    || (head - start) > 4
	    || !isdigit(*(head + 1)))
		return -2;

	/* Extract major version number. */
	if (revstr2num(start, head, &version->major)) return -3; /* NaN */

	start = head + 1;

	/* Find next ' '. */
	while (*head && *head != ' ') head++;
	/* Sanity check. */
	if (!*head || !(head - start) || (head - start) > 4) return -4;

	/* Extract minor version number. */
	if (revstr2num(start, head, &version->minor)) return -5; /* NaN */

	/* Ignore spaces. */
	while (*head == ' ') head++;

	/* Sanity check for code. */
	if (head[0] < '1' || head[0] > '9' ||
	    !isdigit(head[1]) ||
	    !isdigit(head[2]))
		return -6; /* Invalid code. */

	/* Extract code. */
	*code = (head[0] - '0') * 100 + (head[1] - '0') * 10 + head[2] - '0';

	return 0;
}

static int
check_http_server_bugs(struct uri *uri, struct http_connection_info *http,
		       unsigned char *head)
{
	unsigned char *server;
	const unsigned char *const *s;
	static const unsigned char *const buggy_servers[] = {
		"mod_czech/3.1.0",
		"Purveyor",
		"Netscape-Enterprise",
		NULL
	};

	if (!get_opt_bool("protocol.http.bugs.allow_blacklist")
	    || HTTP_1_0(http->sent_version))
		return 0;

	server = parse_header(head, "Server", NULL);
	if (!server)
		return 0;

	for (s = buggy_servers; *s; s++) {
		if (strstr(server, *s)) {
			add_blacklist_entry(uri, SERVER_BLACKLIST_HTTP10);
			break;
		}
	}

	mem_free(server);
	return (*s != NULL);
}

static void
http_end_request(struct connection *conn, struct connection_state state,
		 int notrunc)
{
	shutdown_connection_stream(conn);

	if (conn->info && !((struct http_connection_info *) conn->info)->close
	    && (!conn->socket->ssl) /* We won't keep alive ssl connections */
	    && (!get_opt_bool("protocol.http.bugs.post_no_keepalive")
		|| !conn->uri->post)) {
		if (is_in_state(state, S_OK) && conn->cached)
			normalize_cache_entry(conn->cached, !notrunc ? conn->from : -1);
		set_connection_state(conn, state);
		add_keepalive_connection(conn, HTTP_KEEPALIVE_TIMEOUT, NULL);
	} else {
		abort_connection(conn, state);
	}
}

static void http_send_header(struct socket *);

void
http_protocol_handler(struct connection *conn)
{
	/* setcstate(conn, S_CONN); */

	if (!has_keepalive_connection(conn)) {
		make_connection(conn->socket, conn->uri, http_send_header,
				conn->cache_mode >= CACHE_MODE_FORCE_RELOAD);
	} else {
		http_send_header(conn->socket);
	}
}

void
proxy_protocol_handler(struct connection *conn)
{
	http_protocol_handler(conn);
}

#define IS_PROXY_URI(x) ((x)->protocol == PROTOCOL_PROXY)

#define connection_is_https_proxy(conn) \
	(IS_PROXY_URI((conn)->uri) && (conn)->proxied_uri->protocol == PROTOCOL_HTTPS)

struct http_connection_info *
init_http_connection_info(struct connection *conn, int major, int minor, int close)
{
	struct http_connection_info *http;

	http = mem_calloc(1, sizeof(*http));
	if (!http) {
		http_end_request(conn, connection_state(S_OUT_OF_MEM), 0);
		return NULL;
	}

	http->sent_version.major = major;
	http->sent_version.minor = minor;
	http->close = close;

	/* The CGI code uses this too and blacklisting expects a host name. */
	if (conn->proxied_uri->protocol != PROTOCOL_FILE)
		http->bl_flags = get_blacklist_flags(conn->proxied_uri);

	if (http->bl_flags & SERVER_BLACKLIST_HTTP10
	    || get_opt_bool("protocol.http.bugs.http10")) {
		http->sent_version.major = 1;
		http->sent_version.minor = 0;
	}

	/* If called from HTTPS proxy connection the connection info might have
	 * already been allocated. */
	mem_free_set(&conn->info, http);

	return http;
}

static void
accept_encoding_header(struct string *header)
{
#if defined(CONFIG_GZIP) || defined(CONFIG_BZIP2) || defined(CONFIG_LZMA)
	int comma = 0;

	add_to_string(header, "Accept-Encoding: ");

#ifdef CONFIG_BZIP2
	add_to_string(header, "bzip2");
	comma = 1;
#endif

#ifdef CONFIG_GZIP
	if (comma) add_to_string(header, ", ");
	add_to_string(header, "deflate, gzip");
	comma = 1;
#endif

#ifdef CONFIG_LZMA
	if (comma) add_to_string(header, ", ");
	add_to_string(header, "lzma");
#endif
	add_crlf_to_string(header);
#endif
}

static void
http_send_header(struct socket *socket)
{
	struct connection *conn = socket->conn;
	struct http_connection_info *http;
	int trace = get_opt_bool("protocol.http.trace");
	struct string header;
	unsigned char *post_data = NULL;
	struct auth_entry *entry = NULL;
	struct uri *uri = conn->proxied_uri; /* Set to the real uri */
	unsigned char *optstr;
	int use_connect, talking_to_proxy;

	/* Sanity check for a host */
	if (!uri || !uri->host || !*uri->host || !uri->hostlen) {
		http_end_request(conn, connection_state(S_BAD_URL), 0);
		return;
	}

	http = init_http_connection_info(conn, 1, 1, 0);
	if (!http) return;

	if (!init_string(&header)) {
		http_end_request(conn, connection_state(S_OUT_OF_MEM), 0);
		return;
	}

	if (!conn->cached) conn->cached = find_in_cache(uri);

	talking_to_proxy = IS_PROXY_URI(conn->uri) && !conn->socket->ssl;
	use_connect = connection_is_https_proxy(conn) && !conn->socket->ssl;

	if (trace) {
		add_to_string(&header, "TRACE ");
	} else if (use_connect) {
		add_to_string(&header, "CONNECT ");
		/* In CONNECT requests, we send only a subset of the
		 * headers to the proxy.  See the "CONNECT:" comments
		 * below.  After the CONNECT request succeeds, we
		 * negotiate TLS with the real server and make a new
		 * HTTP request that includes all the headers.  */
	} else if (uri->post) {
		add_to_string(&header, "POST ");
		conn->unrestartable = 1;
	} else {
		add_to_string(&header, "GET ");
	}

	if (!talking_to_proxy) {
		add_char_to_string(&header, '/');
	}

	if (use_connect) {
		/* Add port if it was specified or the default port */
		add_uri_to_string(&header, uri, URI_HTTP_CONNECT);
	} else {
		if (connection_is_https_proxy(conn) && conn->socket->ssl) {
			add_url_to_http_string(&header, uri, URI_DATA);

		} else if (talking_to_proxy) {
			add_url_to_http_string(&header, uri, URI_PROXY);

		} else {
			add_url_to_http_string(&header, conn->uri, URI_DATA);
		}
	}

	add_to_string(&header, " HTTP/");
	add_long_to_string(&header, http->sent_version.major);
	add_char_to_string(&header, '.');
	add_long_to_string(&header, http->sent_version.minor);
	add_crlf_to_string(&header);

	/* CONNECT: Sending a Host header seems pointless as the same
	 * information is already in the CONNECT line.  It's harmless
	 * though and Mozilla does it too.  */
	add_to_string(&header, "Host: ");
	add_uri_to_string(&header, uri, URI_HTTP_HOST);
	add_crlf_to_string(&header);

	/* CONNECT: Proxy-Authorization is intended to be seen by the proxy.  */
	if (talking_to_proxy) {
		unsigned char *user = get_opt_str("protocol.http.proxy.user");
		unsigned char *passwd = get_opt_str("protocol.http.proxy.passwd");

		if (proxy_auth.digest) {
			unsigned char *response;
			int userlen = int_min(strlen(user), AUTH_USER_MAXLEN - 1);
			int passwordlen = int_min(strlen(passwd), AUTH_PASSWORD_MAXLEN - 1);

			if (userlen)
				memcpy(proxy_auth.user, user, userlen);
			proxy_auth.user[userlen] = '\0';
			if (passwordlen)
				memcpy(proxy_auth.password, passwd, passwordlen);
			proxy_auth.password[passwordlen] = '\0';

			/* FIXME: @uri is the proxied URI. Maybe the passed URI
			 * should be the proxy URI aka conn->uri. --jonas */
			response = get_http_auth_digest_response(&proxy_auth, uri);
			if (response) {
				add_to_string(&header, "Proxy-Authorization: Digest ");
				add_to_string(&header, response);
				add_crlf_to_string(&header);

				mem_free(response);
			}

		} else {
			if (user[0]) {
				unsigned char *proxy_data;

				proxy_data = straconcat(user, ":", passwd, (unsigned char *) NULL);
				if (proxy_data) {
					unsigned char *proxy_64 = base64_encode(proxy_data);

					if (proxy_64) {
						add_to_string(&header, "Proxy-Authorization: Basic ");
						add_to_string(&header, proxy_64);
						add_crlf_to_string(&header);
						mem_free(proxy_64);
					}
					mem_free(proxy_data);
				}
			}
		}
	}

	/* CONNECT: User-Agent does not reveal anything about the
	 * resource we're fetching, and it may help the proxy return
	 * better error messages.  */
	optstr = get_opt_str("protocol.http.user_agent");
	if (*optstr && strcmp(optstr, " ")) {
		unsigned char *ustr, ts[64] = "";

		add_to_string(&header, "User-Agent: ");

		if (!list_empty(terminals)) {
			unsigned int tslen = 0;
			struct terminal *term = terminals.prev;

			ulongcat(ts, &tslen, term->width, 3, 0);
			ts[tslen++] = 'x';
			ulongcat(ts, &tslen, term->height, 3, 0);
		}
		ustr = subst_user_agent(optstr, VERSION_STRING, system_name,
					ts);

		if (ustr) {
			add_to_string(&header, ustr);
			mem_free(ustr);
		}

		add_crlf_to_string(&header);
	}

	/* CONNECT: Referer probably is a secret page in the HTTPS
	 * server, so don't reveal it to the proxy.  */
	if (!use_connect) {
		switch (get_opt_int("protocol.http.referer.policy")) {
			case REFERER_NONE:
				/* oh well */
				break;

			case REFERER_FAKE:
				optstr = get_opt_str("protocol.http.referer.fake");
				if (!optstr[0]) break;
				add_to_string(&header, "Referer: ");
				add_to_string(&header, optstr);
				add_crlf_to_string(&header);
				break;

			case REFERER_TRUE:
				if (!conn->referrer) break;
				add_to_string(&header, "Referer: ");
				add_url_to_http_string(&header, conn->referrer, URI_HTTP_REFERRER);
				add_crlf_to_string(&header);
				break;

			case REFERER_SAME_URL:
				add_to_string(&header, "Referer: ");
				add_url_to_http_string(&header, uri, URI_HTTP_REFERRER);
				add_crlf_to_string(&header);
				break;
		}
	}

	/* CONNECT: Do send all Accept* headers to the CONNECT proxy,
	 * because they do not reveal anything about the resource
	 * we're going to request via TLS, and they may affect the
	 * error message if the CONNECT request fails.
	 *
	 * If ELinks is ever changed to vary its Accept headers based
	 * on what it intends to do with the returned resource, e.g.
	 * sending "Accept: text/css" when it wants an external
	 * stylesheet, then it should do that only in the inner GET
	 * and not in the outer CONNECT.  */
	add_to_string(&header, "Accept: */*");
	add_crlf_to_string(&header);

	if (get_opt_bool("protocol.http.compression"))
		accept_encoding_header(&header);

	if (!accept_charset) {
		init_accept_charset();
	}

	if (!(http->bl_flags & SERVER_BLACKLIST_NO_CHARSET)
	    && !get_opt_bool("protocol.http.bugs.accept_charset")
	    && accept_charset) {
		add_to_string(&header, accept_charset);
	}

	optstr = get_opt_str("protocol.http.accept_language");
	if (optstr[0]) {
		add_to_string(&header, "Accept-Language: ");
		add_to_string(&header, optstr);
		add_crlf_to_string(&header);
	}
#ifdef CONFIG_NLS
	else if (get_opt_bool("protocol.http.accept_ui_language")) {
		unsigned char *code = language_to_iso639(current_language);

		if (code) {
			add_to_string(&header, "Accept-Language: ");
			add_to_string(&header, code);
			add_crlf_to_string(&header);
		}
	}
#endif

	/* CONNECT: Proxy-Connection is intended to be seen by the
	 * proxy.  If the CONNECT request succeeds, then the proxy
	 * will forward the remainder of the TCP connection to the
	 * origin server, and Proxy-Connection does not matter; but
	 * if the request fails, then Proxy-Connection may matter.  */
	/* FIXME: What about post-HTTP/1.1?? --Zas */
	if (HTTP_1_1(http->sent_version)) {
		if (!IS_PROXY_URI(conn->uri)) {
			add_to_string(&header, "Connection: ");
		} else {
			add_to_string(&header, "Proxy-Connection: ");
		}

		if (!uri->post || !get_opt_bool("protocol.http.bugs.post_no_keepalive")) {
			add_to_string(&header, "Keep-Alive");
		} else {
			add_to_string(&header, "close");
		}
		add_crlf_to_string(&header);
	}

	/* CONNECT: Do not tell the proxy anything we have cached
	 * about the resource.  */
	if (!use_connect && conn->cached) {
		if (!conn->cached->incomplete && conn->cached->head
		    && conn->cache_mode <= CACHE_MODE_CHECK_IF_MODIFIED) {
			if (conn->cached->last_modified) {
				add_to_string(&header, "If-Modified-Since: ");
				add_to_string(&header, conn->cached->last_modified);
				add_crlf_to_string(&header);
			}
			if (conn->cached->etag) {
				add_to_string(&header, "If-None-Match: ");
				add_to_string(&header, conn->cached->etag);
				add_crlf_to_string(&header);
			}
		}
	}

	/* CONNECT: Let's send cache control headers to the proxy too;
	 * they may affect DNS caching.  */
	if (conn->cache_mode >= CACHE_MODE_FORCE_RELOAD) {
		add_to_string(&header, "Pragma: no-cache");
		add_crlf_to_string(&header);
		add_to_string(&header, "Cache-Control: no-cache");
		add_crlf_to_string(&header);
	}

	/* CONNECT: Do not reveal byte ranges to the proxy.  It can't
	 * do anything good with that information anyway.  */
	if (!use_connect && (conn->from || conn->progress->start > 0)) {
		/* conn->from takes precedence. conn->progress.start is set only the first
		 * time, then conn->from gets updated and in case of any retries
		 * etc we have everything interesting in conn->from already. */
		add_to_string(&header, "Range: bytes=");
		add_long_to_string(&header, conn->from ? conn->from : conn->progress->start);
		add_char_to_string(&header, '-');
		add_crlf_to_string(&header);
	}

	/* CONNECT: The Authorization header is for the origin server only.  */
	if (!use_connect) {
#ifdef CONFIG_GSSAPI
		if (http_negotiate_output(uri, &header) != 0)
#endif
			entry = find_auth(uri);
	}

	if (entry) {
		if (entry->digest) {
			unsigned char *response;

			response = get_http_auth_digest_response(entry, uri);
			if (response) {
				add_to_string(&header, "Authorization: Digest ");
				add_to_string(&header, response);
				add_crlf_to_string(&header);

				mem_free(response);
			}

		} else {
			/* RFC2617 section 2 [Basic Authentication Scheme]
			 *
			 * To receive authorization, the client sends the userid
			 * and password, separated by a single colon (":")
			 * character, within a base64 [7] encoded string in the
			 * credentials. */
			unsigned char *id;

			/* Create base64 encoded string. */
			id = straconcat(entry->user, ":", entry->password,
					(unsigned char *) NULL);
			if (id) {
				unsigned char *base64 = base64_encode(id);

				mem_free_set(&id, base64);
			}

			if (id) {
				add_to_string(&header, "Authorization: Basic ");
				add_to_string(&header, id);
				add_crlf_to_string(&header);
				mem_free(id);
			}
		}
	}

	/* CONNECT: Any POST data is for the origin server only.  */
	if (!use_connect && uri->post) {
		/* We search for first '\n' in uri->post to get content type
		 * as set by get_form_uri(). This '\n' is dropped if any
		 * and replaced by correct '\r\n' termination here. */
		unsigned char *postend = strchr(uri->post, '\n');

		if (postend) {
			add_to_string(&header, "Content-Type: ");
			add_bytes_to_string(&header, uri->post, postend - uri->post);
			add_crlf_to_string(&header);
		}

		post_data = postend ? postend + 1 : uri->post;
		add_to_string(&header, "Content-Length: ");
		add_long_to_string(&header, strlen(post_data) / 2);
		add_crlf_to_string(&header);
	}

#ifdef CONFIG_COOKIES
	/* CONNECT: Cookies are for the origin server only.  */
	if (!use_connect) {
		struct string *cookies = send_cookies(uri);

		if (cookies) {
			add_to_string(&header, "Cookie: ");
			add_string_to_string(&header, cookies);
			add_crlf_to_string(&header);
			done_string(cookies);
		}
	}
#endif

	add_crlf_to_string(&header);

	/* CONNECT: Any POST data is for the origin server only.
	 * This was already checked above and post_data is NULL
	 * in that case.  Verified with an assertion below.  */
	if (post_data) {
#define POST_BUFFER_SIZE 4096
		unsigned char *post = post_data;
		unsigned char buffer[POST_BUFFER_SIZE];
		int n = 0;

		assert(!use_connect); /* see comment above */

		while (post[0] && post[1]) {
			int h1, h2;

			h1 = unhx(post[0]);
			assertm(h1 >= 0 && h1 < 16, "h1 in the POST buffer is %d (%d/%c)", h1, post[0], post[0]);
			if_assert_failed h1 = 0;

			h2 = unhx(post[1]);
			assertm(h2 >= 0 && h2 < 16, "h2 in the POST buffer is %d (%d/%c)", h2, post[1], post[1]);
			if_assert_failed h2 = 0;

			buffer[n++] = (h1<<4) + h2;
			post += 2;
			if (n == POST_BUFFER_SIZE) {
				add_bytes_to_string(&header, buffer, n);
				n = 0;
			}
		}

		if (n)
			add_bytes_to_string(&header, buffer, n);
#undef POST_BUFFER_SIZE
	}

	request_from_socket(socket, header.source, header.length,
			    connection_state(S_SENT),
			    SOCKET_END_ONCLOSE, http_got_header);
	done_string(&header);
}


/* This function decompresses the data block given in @data (if it was
 * compressed), which is long @len bytes. The decompressed data block is given
 * back to the world as the return value and its length is stored into
 * @new_len. After this function returns, the caller will discard all the @len
 * input bytes, so this function must use all of them unless an error occurs.
 *
 * In this function, value of either http->chunk_remaining or http->length is
 * being changed (it depends on if chunked mode is used or not).
 *
 * Note that the function is still a little esotheric for me. Don't take it
 * lightly and don't mess with it without grave reason! If you dare to touch
 * this without testing the changes on slashdot, freshmeat and cvsweb
 * (including revision history), don't dare to send me any patches! ;) --pasky
 *
 * This function gotta die. */
static unsigned char *
decompress_data(struct connection *conn, unsigned char *data, int len,
		int *new_len)
{
	struct http_connection_info *http = conn->info;
	enum { NORMAL, FINISHING } state = NORMAL;
	int did_read = 0;
	int *length_of_block;
	unsigned char *output = NULL;

#define BIG_READ 655360

	if (http->length == LEN_CHUNKED) {
		if (http->chunk_remaining == CHUNK_ZERO_SIZE)
			state = FINISHING;
		length_of_block = &http->chunk_remaining;
	} else {
		length_of_block = &http->length;
		if (!*length_of_block) {
			/* Going to finish this decoding bussiness. */
			state = FINISHING;
		}
	}

	if (conn->content_encoding == ENCODING_NONE) {
		*new_len = len;
		if (*length_of_block > 0) *length_of_block -= len;
		return data;
	}

	*new_len = 0; /* new_len must be zero if we would ever return NULL */

	if (conn->stream_pipes[0] == -1
	    && (c_pipe(conn->stream_pipes) < 0
		|| set_nonblocking_fd(conn->stream_pipes[0]) < 0
		|| set_nonblocking_fd(conn->stream_pipes[1]) < 0)) {
		return NULL;
	}

	do {
		unsigned char *tmp;

		if (state == NORMAL) {
			/* ... we aren't finishing yet. */
			int written = safe_write(conn->stream_pipes[1], data, len);

			if (written >= 0) {
				data += written;
				len -= written;

				/* In non-keep-alive connections http->length == -1, so the test below */
				if (*length_of_block > 0)
					*length_of_block -= written;
				/* http->length is 0 at the end of block for all modes: keep-alive,
				 * non-keep-alive and chunked */
				if (!http->length) {
					/* That's all, folks - let's finish this. */
					state = FINISHING;
				} else if (!len) {
					/* We've done for this round (but not done
					 * completely). Thus we will get out with
					 * what we have and leave what we wrote to
					 * the next round - we have to do that since
					 * we MUST NOT ever empty the pipe completely
					 * - this would cause a disaster for
					 * read_encoded(), which would simply not
					 * work right then. */
					return output;
				}
			}
		}

		if (!conn->stream) {
			conn->stream = open_encoded(conn->stream_pipes[0],
					conn->content_encoding);
			if (!conn->stream) return NULL;
		}

		tmp = mem_realloc(output, *new_len + BIG_READ);
		if (!tmp) break;
		output = tmp;

		did_read = read_encoded(conn->stream, output + *new_len, BIG_READ);

		/* Do not break from the loop if did_read == 0.  It
		 * means no decoded data is available yet, but some may
		 * become available later.  This happens especially with
		 * the bzip2 decoder, which needs an entire compressed
		 * block as input before it generates any output.  */
		if (did_read < 0) {
			state = FINISHING;
			break;
		}
		*new_len += did_read;
	} while (len || (did_read == BIG_READ));

	if (state == FINISHING) shutdown_connection_stream(conn);
	return output;
#undef BIG_READ
}

static int
is_line_in_buffer(struct read_buffer *rb)
{
	int l;

	for (l = 0; l < rb->length; l++) {
		unsigned char a0 = rb->data[l];

		if (a0 == ASCII_LF)
			return l + 1;
		if (a0 == ASCII_CR) {
			if (rb->data[l + 1] == ASCII_LF
			    && l < rb->length - 1)
				return l + 2;
			if (l == rb->length - 1)
				return 0;
		}
		if (a0 < ' ')
			return -1;
	}
	return 0;
}

static void read_http_data(struct socket *socket, struct read_buffer *rb);

static void
read_more_http_data(struct connection *conn, struct read_buffer *rb,
                    int already_got_anything)
{
	struct connection_state state = already_got_anything
		? connection_state(S_TRANS) : conn->state;

	read_from_socket(conn->socket, rb, state, read_http_data);
}

static void
read_http_data_done(struct connection *conn)
{
	struct http_connection_info *http = conn->info;

	/* There's no content but an error so just print
	 * that instead of nothing. */
	if (!conn->from) {
		if (http->code >= 400) {
			http_error_document(conn, http->code);

		} else {
			/* This is not an error, thus fine. No need generate any
			 * document, as this may be empty and it's not a problem.
			 * In case of 3xx, we're probably just getting kicked to
			 * another page anyway. And in case of 2xx, the document
			 * may indeed be empty and thus the user should see it so. */
		}
	}

	http_end_request(conn, connection_state(S_OK), 0);
}

/* Returns:
 * -1 on error
 * 0 if more to read
 * 1 if done
 */
static int
read_chunked_http_data(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *http = conn->info;
	int total_data_len = 0;

	while (1) {
		/* Chunked. Good luck! */
		/* See RFC2616, section 3.6.1. Basically, it looks like:
		 * 1234 ; a = b ; c = d\r\n
		 * aklkjadslkfjalkfjlkajkljfdkljdsfkljdf*1234\r\n
		 * 0\r\n
		 * \r\n */
		if (http->chunk_remaining == CHUNK_DATA_END) {
			int l = is_line_in_buffer(rb);

			if (l) {
				if (l == -1) {
					/* Invalid character in buffer. */
					return -1;
				}

				/* Remove everything to the EOLN. */
				kill_buffer_data(rb, l);
				if (l <= 2) {
					/* Empty line. */
					return 2;
				}
				continue;
			}

		} else if (http->chunk_remaining == CHUNK_SIZE) {
			int l = is_line_in_buffer(rb);

			if (l) {
				unsigned char *de;
				int n = 0;

				if (l != -1) {
					errno = 0;
					n = strtol(rb->data, (char **) &de, 16);
					if (errno || !*de) {
						return -1;
					}
				}

				if (l == -1 || de == rb->data) {
					return -1;
				}

				/* Remove everything to the EOLN. */
				kill_buffer_data(rb, l);
				http->chunk_remaining = n;
				if (!http->chunk_remaining)
					http->chunk_remaining = CHUNK_ZERO_SIZE;
				continue;
			}

		} else {
			unsigned char *data;
			int data_len;
			int zero = (http->chunk_remaining == CHUNK_ZERO_SIZE);
			int len = zero ? 0 : http->chunk_remaining;

			/* Maybe everything necessary didn't come yet.. */
			int_upper_bound(&len, rb->length);
			conn->received += len;

			data = decompress_data(conn, rb->data, len, &data_len);

			if (add_fragment(conn->cached, conn->from,
					 data, data_len) == 1)
				conn->tries = 0;

			if (data && data != rb->data) mem_free(data);

			conn->from += data_len;
			total_data_len += data_len;

			kill_buffer_data(rb, len);

			if (zero) {
				/* Last chunk has zero length, so this is last
				 * chunk, we finished decompression just now
				 * and now we can happily finish reading this
				 * stuff. */
				http->chunk_remaining = CHUNK_DATA_END;
				continue;
			}

			if (!http->chunk_remaining && rb->length > 0) {
				/* Eat newline succeeding each chunk. */
				if (rb->data[0] == ASCII_LF) {
					kill_buffer_data(rb, 1);
				} else {
					if (rb->data[0] != ASCII_CR
					    || (rb->length >= 2
						&& rb->data[1] != ASCII_LF)) {
						return -1;
					}
					if (rb->length < 2) break;
					kill_buffer_data(rb, 2);
				}
				http->chunk_remaining = CHUNK_SIZE;
				continue;
			}
		}
		break;
	}

	/* More to read. */
	return !!total_data_len;
}

/* Returns 0 if more data, 1 if done. */
static int
read_normal_http_data(struct connection *conn, struct read_buffer *rb)
{
	struct http_connection_info *http = conn->info;
	unsigned char *data;
	int data_len;
	int len = rb->length;

	if (http->length >= 0 && http->length < len) {
		/* We won't read more than we have to go. */
		len = http->length;
	}

	conn->received += len;

	data = decompress_data(conn, rb->data, len, &data_len);

	if (add_fragment(conn->cached, conn->from, data, data_len) == 1)
		conn->tries = 0;

	if (data && data != rb->data) mem_free(data);

	conn->from += data_len;

	kill_buffer_data(rb, len);

	if (!http->length && (conn->socket->state == SOCKET_RETRY_ONCLOSE
		|| conn->socket->state == SOCKET_CLOSED)) {
		return 2;
	}

	return !!data_len;
}

static void
read_http_data(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct http_connection_info *http = conn->info;
	int ret;

	if (socket->state == SOCKET_CLOSED) {
		if (conn->content_encoding) {
			/* Flush decompression first. */
			http->length = 0;
		} else {
			read_http_data_done(conn);
			return;
		}
	}

	if (http->length != LEN_CHUNKED) {
		ret = read_normal_http_data(conn, rb);

	} else {
		ret = read_chunked_http_data(conn, rb);
	}

	switch (ret) {
	case 0:
		read_more_http_data(conn, rb, 0);
		break;
	case 1:
		read_more_http_data(conn, rb, 1);
		break;
	case 2:
		read_http_data_done(conn);
		break;
	default:
		assertm(ret == -1, "Unexpected return value: %d", ret);
		abort_connection(conn, connection_state(S_HTTP_ERROR));
	}
}

/* Returns offset of the header end, zero if more data is needed, -1 when
 * incorrect data was received, -2 if this is HTTP/0.9 and no header is to
 * come. */
static int
get_header(struct read_buffer *rb)
{
	int i;

	/* XXX: We will have to do some guess about whether an HTTP header is
	 * coming or not, in order to support HTTP/0.9 reply correctly. This
	 * means a little code duplication with get_http_code(). --pasky */
	if (rb->length > 4 && c_strncasecmp(rb->data, "HTTP/", 5))
		return -2;

	for (i = 0; i < rb->length; i++) {
		unsigned char a0 = rb->data[i];
		unsigned char a1 = rb->data[i + 1];

		if (a0 == 0) {
			rb->data[i] = ' ';
			continue;
		}
		if (a0 == ASCII_LF && a1 == ASCII_LF
		    && i < rb->length - 1)
			return i + 2;
		if (a0 == ASCII_CR && i < rb->length - 3) {
			if (a1 == ASCII_CR) continue;
			if (a1 != ASCII_LF) return -1;
			if (rb->data[i + 2] == ASCII_CR) {
				if (rb->data[i + 3] != ASCII_LF) return -1;
				return i + 4;
			}
		}
	}

	return 0;
}

/* returns 1 if we need retry the connection (for negotiate-auth only) */
static int
check_http_authentication(struct connection *conn, struct uri *uri,
		unsigned char *header, unsigned char *header_field)
{
	unsigned char *str, *d;
	int ret = 0;

	d = parse_header(header, header_field, &str);
	while (d) {
		if (!c_strncasecmp(d, "Basic", 5)) {
			unsigned char *realm = get_header_param(d, "realm");

			if (realm) {
				add_auth_entry(uri, realm, NULL, NULL, 0);
				mem_free(realm);
				mem_free(d);
				break;
			}
		} else if (!c_strncasecmp(d, "Digest", 6)) {
			unsigned char *realm = get_header_param(d, "realm");
			unsigned char *nonce = get_header_param(d, "nonce");
			unsigned char *opaque = get_header_param(d, "opaque");

			add_auth_entry(uri, realm, nonce, opaque, 1);

			mem_free_if(realm);
			mem_free_if(nonce);
			mem_free_if(opaque);
			mem_free(d);
			break;
		}
#ifdef CONFIG_GSSAPI
		else if (!c_strncasecmp(d, HTTPNEG_GSS_STR, HTTPNEG_GSS_STRLEN)) {
			if (http_negotiate_input(conn, uri, HTTPNEG_GSS, str)==0)
				ret = 1;
			mem_free(d);
			break;
		}
		else if (!c_strncasecmp(d, HTTPNEG_NEG_STR, HTTPNEG_NEG_STRLEN)) {
			if (http_negotiate_input(conn, uri, HTTPNEG_NEG, str)==0)
				ret = 1;
			mem_free(d);
			break;
		}
#endif
		mem_free(d);
		d = parse_header(str, header_field, &str);
	}
	return ret;
}


void
http_got_header(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct http_connection_info *http = conn->info;
	unsigned char *head;
#ifdef CONFIG_COOKIES
	unsigned char *cookie, *ch;
#endif
	unsigned char *d;
	struct uri *uri = conn->proxied_uri; /* Set to the real uri */
	struct http_version version = { 0, 9 };
	struct connection_state state = (!is_in_state(conn->state, S_PROC)
					 ? connection_state(S_GETH)
					 : connection_state(S_PROC));
	int a, h = 200;
	int cf;

	if (socket->state == SOCKET_CLOSED) {
		if (!conn->tries && uri->host) {
			if (http->bl_flags & SERVER_BLACKLIST_NO_CHARSET) {
				del_blacklist_entry(uri, SERVER_BLACKLIST_NO_CHARSET);
			} else {
				add_blacklist_entry(uri, SERVER_BLACKLIST_NO_CHARSET);
				conn->tries = -1;
			}
		}
		retry_connection(conn, connection_state(S_CANT_READ));
		return;
	}
	socket->state = SOCKET_RETRY_ONCLOSE;

again:
	a = get_header(rb);
	if (a == -1) {
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}
	if (!a) {
		read_from_socket(conn->socket, rb, state, http_got_header);
		return;
	}
	/* a == -2 from get_header means HTTP/0.9.  In that case, skip
	 * the get_http_code call; @h and @version have already been
	 * initialized with the right values.  */
	if (a == -2) a = 0;
	if ((a && get_http_code(rb, &h, &version))
	    || h == 101) {
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}

	/* When no header, HTTP/0.9 document. That's always text/html,
	 * according to
	 * http://www.w3.org/Protocols/HTTP/AsImplemented.html. */
	/* FIXME: This usage of fake protocol headers for setting up the
	 * content type has been obsoleted by the @content_type member of
	 * {struct cache_entry}. */
	head = (a ? memacpy(rb->data, a)
		  : stracpy("\r\nContent-Type: text/html\r\n"));
	if (!head) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (check_http_server_bugs(uri, http, head)) {
		mem_free(head);
		retry_connection(conn, connection_state(S_RESTART));
		return;
	}

#ifdef CONFIG_CGI
	if (uri->protocol == PROTOCOL_FILE) {
		/* ``Status'' is not a standard HTTP header field although some
		 * HTTP servers like www.php.net uses it for some reason. It should
		 * only be used for CGI scripts so that it does not interfere
		 * with status code depended handling for ``normal'' HTTP like
		 * redirects. */
		d = parse_header(head, "Status", NULL);
		if (d) {
			int h2 = atoi(d);

			mem_free(d);
			if (h2 >= 100 && h2 < 600) h = h2;
			if (h == 101) {
				mem_free(head);
				abort_connection(conn, connection_state(S_HTTP_ERROR));
				return;
			}
		}
	}
#endif

#ifdef CONFIG_COOKIES
	ch = head;
	while ((cookie = parse_header(ch, "Set-Cookie", &ch))) {
		set_cookie(uri, cookie);
		mem_free(cookie);
	}
#endif
	http->code = h;

	if (h == 100) {
		mem_free(head);
		state = connection_state(S_PROC);
		kill_buffer_data(rb, a);
		goto again;
	}
	if (h < 200) {
		mem_free(head);
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}
	if (h == 304) {
		mem_free(head);
		http_end_request(conn, connection_state(S_OK), 1);
		return;
	}
	if (h == 204) {
		mem_free(head);
		http_end_request(conn, connection_state(S_HTTP_204), 0);
		return;
	}
	if (h == 200 && connection_is_https_proxy(conn) && !conn->socket->ssl) {
#ifdef CONFIG_SSL
		mem_free(head);
		socket->need_ssl = 1;
		complete_connect_socket(socket, uri, http_send_header);
#else
		abort_connection(conn, connection_state(S_SSL_ERROR));
#endif
		return;
	}

	conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		mem_free(head);
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	conn->cached->cgi = conn->cgi;
	mem_free_set(&conn->cached->head, head);

	if (!get_opt_bool("document.cache.ignore_cache_control")) {
		struct cache_entry *cached = conn->cached;

		/* I am not entirely sure in what order we should process these
		 * headers and if we should still process Cache-Control max-age
		 * if we already set max age to date mentioned in Expires.
		 * --jonas */
		/* Ensure that when ever cached->max_age is set, cached->expired
		 * is also set, so the cache management knows max_age contains a
		 * valid time. If on the other hand no caching is requested
		 * cached->expire should be set to zero.  */
		if ((d = parse_header(cached->head, "Expires", NULL))) {
			/* Convert date to seconds. */
			time_t expires = parse_date(&d, NULL, 0, 1);

			mem_free(d);

			if (expires && cached->cache_mode != CACHE_MODE_NEVER) {
				timeval_from_seconds(&cached->max_age, expires);
				cached->expire = 1;
			}
		}

		if ((d = parse_header(cached->head, "Pragma", NULL))) {
			if (strstr(d, "no-cache")) {
				cached->cache_mode = CACHE_MODE_NEVER;
				cached->expire = 0;
			}
			mem_free(d);
		}

		if (cached->cache_mode != CACHE_MODE_NEVER
		    && (d = parse_header(cached->head, "Cache-Control", NULL))) {
			if (strstr(d, "no-cache") || strstr(d, "must-revalidate")) {
				cached->cache_mode = CACHE_MODE_NEVER;
				cached->expire = 0;

			} else  {
				unsigned char *pos = strstr(d, "max-age=");

				assert(cached->cache_mode != CACHE_MODE_NEVER);

				if (pos) {
					/* Grab the number of seconds. */
					timeval_T max_age;

					timeval_from_seconds(&max_age, atol(pos + 8));
					timeval_now(&cached->max_age);
					timeval_add_interval(&cached->max_age, &max_age);

					cached->expire = 1;
				}
			}

			mem_free(d);
		}
	}

	/* XXX: Is there some reason why NOT to follow the Location header
	 * for any status? If the server didn't mean it, it wouldn't send
	 * it, after all...? --pasky */
	if (h == 201 || h == 301 || h == 302 || h == 303 || h == 307) {
		d = parse_header(conn->cached->head, "Location", NULL);
		if (d) {
			int use_get_method = (h == 303);

			/* A note from RFC 2616 section 10.3.3:
			 * RFC 1945 and RFC 2068 specify that the client is not
			 * allowed to change the method on the redirected
			 * request. However, most existing user agent
			 * implementations treat 302 as if it were a 303
			 * response, performing a GET on the Location
			 * field-value regardless of the original request
			 * method. */
			/* So POST must not be redirected to GET, but some
			 * BUGGY message boards rely on it :-( */
	    		if (h == 302
			    && get_opt_bool("protocol.http.bugs.broken_302_redirect"))
				use_get_method = 1;

			redirect_cache(conn->cached, d, use_get_method, -1);
			mem_free(d);
		}
	}

	if (h == 401) {
		if (check_http_authentication(conn, uri,
				conn->cached->head, "WWW-Authenticate")) {
			retry_connection(conn, connection_state(S_RESTART));
			return;
		}

	}
	if (h == 407) {
		unsigned char *str;

		d = parse_header(conn->cached->head, "Proxy-Authenticate", &str);
		while (d) {
			if (!c_strncasecmp(d, "Basic", 5)) {
				unsigned char *realm = get_header_param(d, "realm");

				if (realm) {
					mem_free_set(&proxy_auth.realm, realm);
					proxy_auth.digest = 0;
					mem_free(d);
					break;
				}

			} else if (!c_strncasecmp(d, "Digest", 6)) {
				unsigned char *realm = get_header_param(d, "realm");
				unsigned char *nonce = get_header_param(d, "nonce");
				unsigned char *opaque = get_header_param(d, "opaque");

				mem_free_set(&proxy_auth.realm, realm);
				mem_free_set(&proxy_auth.nonce, nonce);
				mem_free_set(&proxy_auth.opaque, opaque);
				proxy_auth.digest = 1;

				mem_free(d);
				break;
			}

			mem_free(d);
			d = parse_header(str, "Proxy-Authenticate", &str);
		}
	}

	kill_buffer_data(rb, a);
	http->close = 0;
	http->length = -1;
	http->recv_version = version;

	if ((d = parse_header(conn->cached->head, "Connection", NULL))
	     || (d = parse_header(conn->cached->head, "Proxy-Connection", NULL))) {
		if (!c_strcasecmp(d, "close")) http->close = 1;
		mem_free(d);
	} else if (PRE_HTTP_1_1(version)) {
		http->close = 1;
	}

	cf = conn->from;
	conn->from = 0;
	d = parse_header(conn->cached->head, "Content-Range", NULL);
	if (d) {
		if (strlen(d) > 6) {
			d[5] = 0;
			if (isdigit(d[6]) && !c_strcasecmp(d, "bytes")) {
				int f;

				errno = 0;
				f = strtol(d + 6, NULL, 10);

				if (!errno && f >= 0) conn->from = f;
			}
		}
		mem_free(d);
	}
	if (cf && !conn->from && !conn->unrestartable) conn->unrestartable = 1;
	if ((conn->progress->start <= 0 && conn->from > cf) || conn->from < 0) {
		/* We don't want this if conn->progress.start because then conn->from will
		 * be probably value of conn->progress.start, while cf is 0. */
		abort_connection(conn, connection_state(S_HTTP_ERROR));
		return;
	}

#if 0
	{
		struct status *s;
		foreach (s, conn->downloads) {
			fprintf(stderr, "conn %p status %p pri %d st %d er %d :: ce %s",
				conn, s, s->pri, s->state, s->prev_error,
				s->cached ? s->cached->url : (unsigned char *) "N-U-L-L");
		}
	}
#endif

	if (conn->progress->start >= 0) {
		/* Update to the real value which we've got from Content-Range. */
		conn->progress->seek = conn->from;
	}
	conn->progress->start = conn->from;

	d = parse_header(conn->cached->head, "Content-Length", NULL);
	if (d) {
		unsigned char *ep;
		int l;

		errno = 0;
		l = strtol(d, (char **) &ep, 10);

		if (!errno && !*ep && l >= 0) {
			if (!http->close || POST_HTTP_1_0(version))
				http->length = l;
			conn->est_length = conn->from + l;
		}
		mem_free(d);
	}

	if (!conn->unrestartable) {
		d = parse_header(conn->cached->head, "Accept-Ranges", NULL);

		if (d) {
			if (!c_strcasecmp(d, "none"))
				conn->unrestartable = 1;
			mem_free(d);
		} else {
			if (!conn->from)
				conn->unrestartable = 1;
		}
	}

	d = parse_header(conn->cached->head, "Transfer-Encoding", NULL);
	if (d) {
		if (!c_strcasecmp(d, "chunked")) {
			http->length = LEN_CHUNKED;
			http->chunk_remaining = CHUNK_SIZE;
		}
		mem_free(d);
	}
	if (!http->close && http->length == -1) http->close = 1;

	d = parse_header(conn->cached->head, "Last-Modified", NULL);
	if (d) {
		if (conn->cached->last_modified && c_strcasecmp(conn->cached->last_modified, d)) {
			delete_entry_content(conn->cached);
			if (conn->from) {
				conn->from = 0;
				mem_free(d);
				retry_connection(conn, connection_state(S_MODIFIED));
				return;
			}
		}
		if (!conn->cached->last_modified) conn->cached->last_modified = d;
		else mem_free(d);
	}
	if (!conn->cached->last_modified) {
		d = parse_header(conn->cached->head, "Date", NULL);
		if (d) conn->cached->last_modified = d;
	}

	/* FIXME: Parse only if HTTP/1.1 or later? --Zas */
	d = parse_header(conn->cached->head, "ETag", NULL);
	if (d) {
		if (conn->cached->etag) {
			unsigned char *old_tag = conn->cached->etag;
			unsigned char *new_tag = d;

			/* http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.19 */

			if (new_tag[0] == 'W' && new_tag[1] == '/')
				new_tag += 2;

			if (old_tag[0] == 'W' && old_tag[1] == '/')
				old_tag += 2;

			if (strcmp(new_tag, old_tag)) {
				delete_entry_content(conn->cached);
				if (conn->from) {
					conn->from = 0;
					mem_free(d);
					retry_connection(conn, connection_state(S_MODIFIED));
					return;
				}
			}
		}

		if (!conn->cached->etag)
			conn->cached->etag = d;
		else
			mem_free(d);
	}

	d = parse_header(conn->cached->head, "Content-Encoding", NULL);
	if (d) {
		unsigned char *extension = get_extension_from_uri(uri);
		enum stream_encoding file_encoding;

		file_encoding = extension ? guess_encoding(extension) : ENCODING_NONE;
		mem_free_if(extension);

		/* If the content is encoded, we want to preserve the encoding
		 * if it is implied by the extension, so that saving the URI
		 * will leave the saved file with the correct encoding. */
#ifdef CONFIG_GZIP
		if (file_encoding != ENCODING_GZIP
		    && (!c_strcasecmp(d, "gzip") || !c_strcasecmp(d, "x-gzip")))
		    	conn->content_encoding = ENCODING_GZIP;
		if (!c_strcasecmp(d, "deflate") || !c_strcasecmp(d, "x-deflate"))
			conn->content_encoding = ENCODING_DEFLATE;
#endif

#ifdef CONFIG_BZIP2
		if (file_encoding != ENCODING_BZIP2
		    && (!c_strcasecmp(d, "bzip2") || !c_strcasecmp(d, "x-bzip2")))
			conn->content_encoding = ENCODING_BZIP2;
#endif

#ifdef CONFIG_LZMA
		if (file_encoding != ENCODING_LZMA
		    && (!c_strcasecmp(d, "lzma") || !c_strcasecmp(d, "x-lzma")))
			conn->content_encoding = ENCODING_LZMA;
#endif
		mem_free(d);
	}

	if (conn->content_encoding != ENCODING_NONE) {
		mem_free_if(conn->cached->encoding_info);
		conn->cached->encoding_info = stracpy(get_encoding_name(conn->content_encoding));
	}

	if (http->length == -1 || http->close)
		socket->state = SOCKET_END_ONCLOSE;

	read_http_data(socket, rb);
}
