/* URL parser and translator; implementation of RFC 2396. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_IDNA_H
#include <idna.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_NETDB_H
#include <netdb.h> /* OS/2 needs this after sys/types.h */
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "main/object.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/string.h"


static inline int
end_of_dir(unsigned char c)
{
	/* This used to check for c == ';' as well.  But section 3.3
	 * of RFC 2396 explicitly says that parameters in a path
	 * segment "are not significant to the parsing of relative
	 * references."  */
	return c == POST_CHAR || c == '#' || c == '?';
}

static inline int
is_uri_dir_sep(const struct uri *uri, unsigned char pos)
{
	return (uri->protocol == PROTOCOL_FILE ? dir_sep(pos) : pos == '/');
}


int
is_in_domain(unsigned char *domain, unsigned char *server, int server_len)
{
	int domain_len = strlen(domain);
	int len;

	if (domain_len > server_len)
		return 0;

	if (domain_len == server_len)
		return !c_strncasecmp(domain, server, server_len);

	len = server_len - domain_len;
	if (server[len - 1] != '.')
		return 0;

	return !c_strncasecmp(domain, server + len, domain_len);
}

int
is_ip_address(const unsigned char *address, int addresslen)
{
	/* The @address has well defined limits so it would be a shame to
	 * allocate it. */
	unsigned char buffer[IP_ADDRESS_BUFFER_SIZE];

	if (addresslen >= sizeof(buffer))
		return 0;

	safe_strncpy(buffer, address, addresslen + 1);

#ifdef HAVE_INET_PTON
#ifdef CONFIG_IPV6
	{
		struct sockaddr_in6 addr6;

		if (inet_pton(AF_INET6, buffer, &addr6.sin6_addr) > 0)
			return 1;
	}
#endif /* CONFIG_IPV6 */
	{
		struct in_addr addr4;

		if (inet_pton(AF_INET, buffer, &addr4) > 0)
			return 1;
	}

	return 0;
#else
	/* FIXME: Is this ever the case? */
	return 0;
#endif /* HAVE_INET_PTON */
}


int
end_with_known_tld(const unsigned char *s, int slen)
{
	int i;
	static const unsigned char *const tld[] =
	{ "com", "edu", "net",
	  "org", "gov", "mil",
	  "int", "biz", "arpa",
	  "aero", "coop",
	  "info", "museum",
	  "name", "pro", NULL };

	if (!slen) return -1;
	if (slen < 0) slen = strlen(s);

	for (i = 0; tld[i]; i++) {
		int tldlen = strlen(tld[i]);
		int pos = slen - tldlen;

		if (pos >= 0 && !c_strncasecmp(&s[pos], tld[i], tldlen))
			return pos;
	}

	return -1;
}

/* XXX: this function writes to @name. */
static int
check_whether_file_exists(unsigned char *name)
{
	/* Check POST_CHAR etc ... */
	static const unsigned char chars[] = POST_CHAR_S "#?";
	int i;
	int namelen = strlen(name);

	if (file_exists(name))
		return namelen;

	for (i = 0; i < sizeof(chars) - 1; i++) {
		unsigned char *pos = memchr(name, chars[i], namelen);
		int exists;

		if (!pos) continue;

		*pos = 0;
		exists = file_exists(name);
		*pos = chars[i];

		if (exists) {
			return pos - name;
		}
	}

	return -1;
}

/* Encodes URIs without encoding stuff like fragments and query separators. */
static void
encode_file_uri_string(struct string *string, unsigned char *uristring)
{
	int filenamelen = check_whether_file_exists(uristring);

	encode_uri_string(string, uristring, filenamelen, 0);
	if (filenamelen > 0) add_to_string(string, uristring + filenamelen);
}


static inline int
get_protocol_length(const unsigned char *url)
{
	unsigned char *end = (unsigned char *) url;

	/* Seek the end of the protocol name if any. */
	/* RFC1738:
	 * scheme  = 1*[ lowalpha | digit | "+" | "-" | "." ]
	 * (but per its recommendations we accept "upalpha" too) */
	while (isalnum(*end) || *end == '+' || *end == '-' || *end == '.')
		end++;

	/* Now we make something to support our "IP version in protocol scheme
	 * name" hack and silently chop off the last digit if it's there. The
	 * IETF's not gonna notice I hope or it'd be going after us hard. */
	if (end != url && isdigit(end[-1]))
		end--;

	/* Also return 0 if there's no protocol name (@end == @url). */
	return (*end == ':' || isdigit(*end)) ? end - url : 0;
}

enum uri_errno
parse_uri(struct uri *uri, unsigned char *uristring)
{
	unsigned char *prefix_end, *host_end;
#ifdef CONFIG_IPV6
	unsigned char *lbracket, *rbracket;
#endif

	assertm(uristring != NULL, "No uri to parse.");
	memset(uri, 0, sizeof(*uri));

	/* Nothing to do for an empty url. */
	if_assert_failed return 0;
	if (!*uristring) return URI_ERRNO_EMPTY;

	uri->string = uristring;
	uri->protocollen = get_protocol_length(uristring);

	/* Invalid */
	if (!uri->protocollen) return URI_ERRNO_INVALID_PROTOCOL;

	/* Figure out whether the protocol is known */
	uri->protocol = get_protocol(struri(uri), uri->protocollen);

	prefix_end = uristring + uri->protocollen; /* ':' */

	/* Check if there's a digit after the protocol name. */
	if (isdigit(*prefix_end)) {
		uri->ip_family = uristring[uri->protocollen] - '0';
		prefix_end++;
	}
	if (*prefix_end != ':')
		return URI_ERRNO_INVALID_PROTOCOL;
	prefix_end++;

	/* Skip slashes */

	if (prefix_end[0] == '/' && prefix_end[1] == '/') {
		if (prefix_end[2] == '/'
		    && get_protocol_need_slash_after_host(uri->protocol))
			return URI_ERRNO_TOO_MANY_SLASHES;

		prefix_end += 2;

	} else if (get_protocol_need_slashes(uri->protocol)) {
		return URI_ERRNO_NO_SLASHES;
	}

	if (get_protocol_free_syntax(uri->protocol)) {
		uri->data = prefix_end;
		uri->datalen = strlen(prefix_end);
		return URI_ERRNO_OK;

	} else if (uri->protocol == PROTOCOL_FILE) {
		int datalen = strcspn(prefix_end, "#" POST_CHAR_S);
		unsigned char *frag_or_post = prefix_end + datalen;

		/* Extract the fragment part. */
		if (datalen >= 0) {
			if (*frag_or_post == '#') {
				uri->fragment = frag_or_post + 1;
				uri->fragmentlen = strcspn(uri->fragment, POST_CHAR_S);
				frag_or_post = uri->fragment + uri->fragmentlen;
			}
			if (*frag_or_post == POST_CHAR) {
				uri->post = frag_or_post + 1;
			}
		} else {
			datalen = strlen(prefix_end);
		}

		/* A bit of a special case, but using the "normal" host
		 * parsing seems a bit scary at this point. (see bug 107). */
		if (datalen > 9 && !c_strncasecmp(prefix_end, "localhost/", 10)) {
			prefix_end += 9;
			datalen -= 9;
		}

		uri->data = prefix_end;
		uri->datalen = datalen;

		return URI_ERRNO_OK;
	}

	/* Isolate host */

#ifdef CONFIG_IPV6
	/* Get brackets enclosing IPv6 address */
	lbracket = strchr(prefix_end, '[');
	if (lbracket) {
		rbracket = strchr(lbracket, ']');
		/* [address] is handled only inside of hostname part (surprisingly). */
		if (rbracket && rbracket < prefix_end + strcspn(prefix_end, "/"))
			uri->ipv6 = 1;
		else
			lbracket = rbracket = NULL;
	} else {
		rbracket = NULL;
	}
#endif

	/* Possibly skip auth part */
	host_end = prefix_end + strcspn(prefix_end, "@");

	if (prefix_end + strcspn(prefix_end, "/") > host_end
	    && *host_end) { /* we have auth info here */
		unsigned char *user_end;

		/* Allow '@' in the password component */
		while (strcspn(host_end + 1, "@") < strcspn(host_end + 1, "/?"))
			host_end = host_end + 1 + strcspn(host_end + 1, "@");

		user_end = strchr(prefix_end, ':');

		if (!user_end || user_end > host_end) {
			uri->user = prefix_end;
			uri->userlen = host_end - prefix_end;
		} else {
			uri->user = prefix_end;
			uri->userlen = user_end - prefix_end;
			uri->password = user_end + 1;
			uri->passwordlen = host_end - user_end - 1;
		}
		prefix_end = host_end + 1;
	}

#ifdef CONFIG_IPV6
	if (uri->ipv6)
		host_end = rbracket + strcspn(rbracket, ":/?");
	else
#endif
		host_end = prefix_end + strcspn(prefix_end, ":/?");

#ifdef CONFIG_IPV6
	if (uri->ipv6) {
		int addrlen = rbracket - lbracket - 1;

		/* Check for valid length.
		 * addrlen >= sizeof(hostbuf) is theorically impossible
		 * but i keep the test in case of... Safer, imho --Zas */
		assertm(addrlen >= 0 && addrlen < NI_MAXHOST,
			"parse_uri(): addrlen value is bad (%d) for URL '%s'. "
			"Problems are likely to be encountered. Please report "
			"this, it is a security bug!", addrlen, uristring);
		if_assert_failed return URI_ERRNO_IPV6_SECURITY;

		uri->host = lbracket + 1;
		uri->hostlen = addrlen;
	} else
#endif
	{
		uri->host = prefix_end;
		uri->hostlen = host_end - prefix_end;

		/* Trim trailing '.'s */
		if (uri->hostlen && uri->host[uri->hostlen - 1] == '.')
			return URI_ERRNO_TRAILING_DOTS;
	}

	if (*host_end == ':') { /* we have port here */
		unsigned char *port_end = host_end + 1 + strcspn(host_end + 1, "/");

		host_end++;

		uri->port = host_end;
		uri->portlen = port_end - host_end;

		if (uri->portlen == 0)
			return URI_ERRNO_NO_PORT_COLON;

		/* We only use 8 bits for portlen so better check */
		if (uri->portlen != port_end - host_end)
			return URI_ERRNO_INVALID_PORT;

		/* test if port is number */
		/* TODO: possibly lookup for the service otherwise? --pasky */
		for (; host_end < port_end; host_end++)
			if (!isdigit(*host_end))
				return URI_ERRNO_INVALID_PORT;

		/* Check valid port value, and let show an error message
		 * about invalid url syntax. */
		if (uri->port && uri->portlen) {
			int n;

			errno = 0;
			n = strtol(uri->port, NULL, 10);
			if (errno || !uri_port_is_valid(n))
				return URI_ERRNO_INVALID_PORT;
		}
	}

	if (*host_end == '/') {
		host_end++;

	} else if (get_protocol_need_slash_after_host(uri->protocol)) {
		/* The need for slash after the host component depends on the
		 * need for a host component. -- The dangerous mind of Jonah */
		if (!uri->hostlen)
			return URI_ERRNO_NO_HOST;

		return URI_ERRNO_NO_HOST_SLASH;
	}

	/* Look for #fragment or POST_CHAR */
	prefix_end = host_end + strcspn(host_end, "#" POST_CHAR_S);
	uri->data = host_end;
	uri->datalen = prefix_end - host_end;

	if (*prefix_end == '#') {
		uri->fragment = prefix_end + 1;
		uri->fragmentlen = strcspn(uri->fragment, POST_CHAR_S);
		prefix_end = uri->fragment + uri->fragmentlen;
	}

	if (*prefix_end == POST_CHAR) {
		uri->post = prefix_end + 1;
	}

	return URI_ERRNO_OK;
}

int
get_uri_port(const struct uri *uri)
{
	if (uri->port && uri->portlen) {
		const unsigned char *end = uri->port;
		int port = strtol(uri->port, (char **) &end, 10);

		if (end != uri->port) {
			assert(uri_port_is_valid(port));
			return port;
		}
	}

	return get_protocol_port(uri->protocol);
}

#define can_compare_uri_components(comp) !(((comp) & (URI_SPECIAL | URI_IDN)))

static inline int
compare_component(const unsigned char *a, int alen,
		  const unsigned char *b, int blen)
{
	/* Check that the length and the strings are both set or unset */
	if (alen != blen || !!a != !!b) return 0;

	/* Both are unset so that will make a perfect match */
	if (!a || !alen) return 1;

	/* Let the higher forces decide */
	return !memcmp(a, b, blen);
}

#define wants(x) (components & (x))

int
compare_uri(const struct uri *a, const struct uri *b,
	    enum uri_component components)
{
	if (a == b) return 1;
	if (!components) return 0;

	assertm(can_compare_uri_components(components),
		"compare_uri() is a work in progress. Component unsupported");

	return (!wants(URI_PROTOCOL) || a->protocol == b->protocol)
		&& (!wants(URI_IP_FAMILY) || a->ip_family == b->ip_family)
		&& (!wants(URI_USER)
		    || compare_component(a->user, a->userlen, b->user, b->userlen))
		&& (!wants(URI_PASSWORD)
		    || compare_component(a->password, a->passwordlen, b->password, b->passwordlen))
		&& (!wants(URI_HOST)
		    || compare_component(a->host, a->hostlen, b->host, b->hostlen))
		&& (!wants(URI_PORT)
		    || compare_component(a->port, a->portlen, b->port, b->portlen))
		&& (!wants(URI_DATA)
		    || compare_component(a->data, a->datalen, b->data, b->datalen))
		&& (!wants(URI_FRAGMENT)
		    || compare_component(a->fragment, a->fragmentlen, b->fragment, b->fragmentlen))
		&& (!wants(URI_POST)
		    || compare_component(a->post, a->post ? strlen(a->post) : 0, b->post, b->post ? strlen(b->post) : 0));
}


/* We might need something more intelligent than this Swiss army knife. */
struct string *
add_uri_to_string(struct string *string, const struct uri *uri,
		  enum uri_component components)
{
	/* Custom or unknown keep the URI untouched. */
	if (uri->protocol == PROTOCOL_UNKNOWN)
		return add_to_string(string, struri(uri));

 	if (wants(URI_PROTOCOL)) {
		add_bytes_to_string(string, uri->string, uri->protocollen);
		if (wants(URI_IP_FAMILY) && uri->ip_family)
			add_long_to_string(string, uri->ip_family);
		add_char_to_string(string, ':');
 		if (get_protocol_need_slashes(uri->protocol))
			add_to_string(string, "//");
 	}

 	if (wants(URI_USER) && uri->userlen) {
		add_bytes_to_string(string, uri->user, uri->userlen);

 		if (wants(URI_PASSWORD) && uri->passwordlen) {
			add_char_to_string(string, ':');
			add_bytes_to_string(string, uri->password,
						    uri->passwordlen);
 		}

		add_char_to_string(string, '@');

	} else if (wants(URI_PASSWORD) && uri->passwordlen) {
		add_bytes_to_string(string, uri->password, uri->passwordlen);
	}

 	if (wants(URI_HOST) && uri->hostlen) {
		int add_host = 1;

#ifdef CONFIG_IPV6
		/* Rationale for wants(URI_PORT): The [notation] was invented
		 * so that you can have an IPv6 addy and a port together. So
		 * we want to use it when that happens, otherwise we need not
		 * bother (that happens only when we want it for DNS anyway).
		 * I insist on an implied elegancy of this way, but YMMV. ;-)
		 * --pasky */
		if (uri->ipv6 && wants(URI_PORT)) add_char_to_string(string, '[');
#endif
#ifdef CONFIG_IDN
		/* Support for the GNU International Domain Name library.
		 *
		 * http://www.gnu.org/software/libidn/manual/html_node/IDNA-Functions.html
		 *
		 * Now it is probably not perfect because idna_to_ascii_lz()
		 * will be using a ``zero terminated input string encoded in
		 * the current locale's character set''. Anyway I don't know
		 * how to convert anything to UTF-8 or Unicode. --jonas */
		if (wants(URI_IDN)) {
			unsigned char *host = memacpy(uri->host, uri->hostlen);

			if (host) {
				char *idname;
				int code = idna_to_ascii_lz(host, &idname, 0);

				/* FIXME: Return NULL if it coughed? --jonas */
				if (code == IDNA_SUCCESS) {
					add_to_string(string, idname);
					free(idname);
					add_host = 0;
				}

				mem_free(host);
			}
		}

#endif
		if (add_host)
			add_bytes_to_string(string, uri->host, uri->hostlen);

#ifdef CONFIG_IPV6
		if (uri->ipv6 && wants(URI_PORT)) add_char_to_string(string, ']');
#endif
 	}

 	if (wants(URI_PORT) || wants(URI_DEFAULT_PORT)) {
 		if (uri->portlen) {
			add_char_to_string(string, ':');
			add_bytes_to_string(string, uri->port, uri->portlen);

		} else if (wants(URI_DEFAULT_PORT)
			   && uri->protocol != PROTOCOL_USER) {
			/* For user protocols we don't know a default port.
			 * Should user protocols ports be configurable? */
			int port = get_protocol_port(uri->protocol);

			add_char_to_string(string, ':');
			add_long_to_string(string, port);
		}
	}

	/* Only add slash if we need to separate */
	if ((wants(URI_DATA) || wants(URI_POST) || components == URI_HTTP_REFERRER_HOST)
	    && wants(~(URI_DATA | URI_PORT))
	    && get_protocol_need_slash_after_host(uri->protocol))
		add_char_to_string(string, '/');

	if (wants(URI_DATA) && uri->datalen)
		add_bytes_to_string(string, uri->data, uri->datalen);

	/* We can not test uri->datalen here since we need to always
	 * add '/'. */
	if (wants(URI_PATH) || wants(URI_FILENAME)) {
		const unsigned char *filename = uri->data;
		const unsigned char *pos;

		assertm(!wants(URI_FILENAME) || components == URI_FILENAME,
			"URI_FILENAME should be used alone %d", components);

		if (wants(URI_PATH) && !is_uri_dir_sep(uri, *filename)) {
#ifdef CONFIG_OS_WIN32
			if (uri->protocol != PROTOCOL_FILE)
#endif
			/* FIXME: Add correct separator */
			add_char_to_string(string, '/');
		}

		if (!uri->datalen) return string;

		for (pos = filename; *pos && !end_of_dir(*pos); pos++)
			if (wants(URI_FILENAME) && is_uri_dir_sep(uri, *pos))
				filename = pos + 1;

		return add_bytes_to_string(string, filename, pos - filename);
	}

	if (wants(URI_QUERY) && uri->datalen) {
		const unsigned char *query = memchr(uri->data, '?', uri->datalen);

		assertm(URI_QUERY == components,
			"URI_QUERY should be used alone %d", components);

		if (!query) return string;

		query++;
		/* Check fragment and POST_CHAR */
		return add_bytes_to_string(string, query, strcspn(query, "#" POST_CHAR_S));
	}

	if (wants(URI_FRAGMENT) && uri->fragmentlen) {
		add_char_to_string(string, '#');
		add_bytes_to_string(string, uri->fragment, uri->fragmentlen);
	}

	if (wants(URI_POST) && uri->post) {
		add_char_to_string(string, POST_CHAR);
		add_to_string(string, uri->post);

	} else if (wants(URI_POST_INFO) && uri->post) {
		if (!strncmp(uri->post, "text/plain", 10)) {
			add_to_string(string, " (PLAIN TEXT DATA)");

		} else if (!strncmp(uri->post, "multipart/form-data;", 20)) {
			add_to_string(string, " (MULTIPART FORM DATA)");

		} else {
			add_to_string(string, " (POST DATA)");
		}

	}

	return string;
}

#undef wants

unsigned char *
get_uri_string(const struct uri *uri, enum uri_component components)
{
	struct string string;

	if (init_string(&string)
	    && add_uri_to_string(&string, uri, components))
		return string.source;

	done_string(&string);
	return NULL;
}


struct string *
add_string_uri_to_string(struct string *string, unsigned char *uristring,
			 enum uri_component components)
{
	struct uri uri;

	if (parse_uri(&uri, uristring) != URI_ERRNO_OK)
		return NULL;

	return add_uri_to_string(string, &uri, components);
}


#define normalize_uri_reparse(str)	normalize_uri(NULL, str)
#define normalize_uri_noparse(uri)	normalize_uri(uri, struri(uri))

unsigned char *
normalize_uri(struct uri *uri, unsigned char *uristring)
{
	unsigned char *parse_string = uristring;
	unsigned char *src, *dest, *path;
	int need_slash = 0, keep_dslash = 1;
	int parse = (uri == NULL);
	struct uri uri_struct;

	if (!uri) uri = &uri_struct;

	/* We need to get the real (proxied) URI but lowercase relevant URI
	 * parts along the way. */
	do {
		if (parse && parse_uri(uri, parse_string) != URI_ERRNO_OK)
			return uristring;

		assert(uri->data);

		/* This is a maybe not the right place but both join_urls() and
		 * get_translated_uri() through translate_url() calls this
		 * function and then it already works on and modifies an
		 * allocated copy. */
		convert_to_lowercase_locale_indep(uri->string, uri->protocollen);
		if (uri->hostlen) convert_to_lowercase_locale_indep(uri->host, uri->hostlen);

		parse = 1;
		parse_string = uri->data;
	} while (uri->protocol == PROTOCOL_PROXY);

	if (get_protocol_free_syntax(uri->protocol))
		return uristring;

	if (uri->protocol != PROTOCOL_UNKNOWN) {
		need_slash = get_protocol_need_slash_after_host(uri->protocol);
		keep_dslash = get_protocol_keep_double_slashes(uri->protocol);
	}

	path = uri->data - need_slash;
	dest = src = path;

	/* This loop mangles the URI string by removing ".." and "." segments.
	 * However it must not alter "//" without reason; see bug 744.  */
	while (*dest) {
		/* If the following pieces are the LAST parts of URL, we remove
		 * them as well. See RFC 2396 section 5.2 for details. */

		if (end_of_dir(src[0])) {
			/* URL data contains no more path. */
			memmove(dest, src, strlen(src) + 1);
			break;
		}

		if (!is_uri_dir_sep(uri, src[0])) {
			/* This is to reduce indentation */

		} else if (src[1] == '.') {
			if (!src[2]) {
				/* /. - skip the dot */
				*dest++ = *src;
				*dest = 0;
				break;

			} else if (is_uri_dir_sep(uri, src[2])) {
				/* /./ - strip that.. */
				src += 2;
				continue;

			} else if (src[2] == '.'
				   && (is_uri_dir_sep(uri, src[3]) || !src[3])) {
				/* /../ or /.. - skip it and preceding element.
				 *
				 * <path> "/foo/bar" <dest> ...
				 * <src> ("/../" or "/..\0") ...
				 *
				 * Remove "bar" and the directory
				 * separator that precedes it.  The
				 * separator will be added back in the
				 * next iteration unless another ".."
				 * follows, in which case it will be
				 * added later.  "bar" may be empty.  */

				while (dest > path) {
					dest--;
					if (is_uri_dir_sep(uri, *dest)) break;
				}

				/* <path> "/foo" <dest> "/bar" ...
				 * <src> ("/../" or "/..\0") ... */
				if (!src[3]) {
					/* /.. - add ending slash and stop */
					*dest++ = *src;
					*dest = 0;
					break;
				}

				src += 3;
				continue;
			}

		} else if (is_uri_dir_sep(uri, src[1]) && !keep_dslash) {
			/* // - ignore first '/'. */
			src += 1;
			continue;
		}

		/* We don't want to access memory past the NUL char. */
		*dest = *src++;
		if (*dest) dest++;
	}

	return uristring;
}

/* The 'file' scheme URI comes in and bastardized URI comes out which consists
 * of just the complete path to file/directory, which the dumb 'file' protocol
 * backend can understand. No host parts etc, that is what this function is
 * supposed to chew. */
static struct uri *
transform_file_url(struct uri *uri, const unsigned char *cwd)
{
	unsigned char *path = uri->data;

	assert(uri->protocol == PROTOCOL_FILE && uri->data);

	/* Sort out the host part. We currently support only host "localhost"
	 * (plus empty host part will be assumed to be "localhost" as well).
	 * As our extensions, '.' will reference to the cwd on localhost
	 * (originally, when the first thing after file:// wasn't "localhost/",
	 * we assumed the cwd as well, and pretended that there's no host part
	 * at all) and '..' to the directory parent to cwd. Another extension
	 * is that if this is a DOS-like system, the first char in two-char
	 * host part is uppercase letter and the second char is a colon, it is
	 * assumed to be a local disk specification. */
	/* TODO: Use FTP for non-localhost hosts. --pasky */

	/* For URL "file://", we open the current directory. Some other
	 * browsers instead open root directory, but AFAIK the standard does
	 * not specify that and this was the original behaviour and it is more
	 * consistent with our file://./ notation. */

	/* Who would name their file/dir '...' ? */
	if (*path == '.' || !*path) {
		struct string dir;

		if (!init_string(&dir))
			return NULL;

		encode_uri_string(&dir, cwd, -1, 0);

		/* Either we will end up with '//' and translate_directories()
		 * will shorten it or the '/' will mark the inserted cwd as a
		 * directory. */
		if (*path == '.') *path = '/';

		/* Insert the current working directory. */
		/* The offset is 7 == sizeof("file://") - 1. */
		insert_in_string(&struri(uri), 7, dir.source, dir.length);

		done_string(&dir);
		return uri;
	}

#ifdef DOS_FS
	if (isasciialpha(path[0]) && path[1] == ':' && dir_sep(path[2]))
		return NULL;
#endif

	for (; *path && !dir_sep(*path); path++);

	/* FIXME: We will in fact assume localhost even for non-local hosts,
	 * until we will support the FTP transformation. --pasky */

	memmove(uri->data, path, strlen(path) + 1);
	return uri;
}

static unsigned char *translate_url(unsigned char *url, unsigned char *cwd);

unsigned char *
join_urls(struct uri *base, unsigned char *rel)
{
	unsigned char *uristring, *path;
	int add_slash = 0;
	int translate = 0;
	int length = 0;

	/* See RFC 1808 */
	/* TODO: Support for ';' ? (see the RFC) --pasky */

	/* For '#', '?' and '//' we could use get_uri_string() but it might be
	 * too expensive since it uses granular allocation scheme. I wouldn't
	 * personally mind tho' because it would be cleaner. --jonas */
	if (rel[0] == '#') {
		/* Strip fragment and post part from the base URI and append
		 * the fragment string in @rel. */
		length  = base->fragment
			? base->fragment - struri(base) - 1
			: get_real_uri_length(base);

	} else if (rel[0] == '?') {
		/* Strip query, fragment and post part from the base URI and
		 * append the query string in @rel. */
		length  = base->fragment ? base->fragment - struri(base) - 1
					 : get_real_uri_length(base);

		uristring = memchr(base->data, '?', base->datalen);
		if (uristring) length = uristring - struri(base);

	} else if (rel[0] == '/' && rel[1] == '/') {
		if (!get_protocol_need_slashes(base->protocol))
			return NULL;

		/* Get `<protocol>:' from the base URI and append the `//' part
		 * from @rel. */
		length = base->protocollen + 1;

		/* We need to sanitize the relative part and add stuff like
		 * host slash. */
		translate = 1;
	}

	/* If one of the tests above set @length to something useful */
	if (length) {
		uristring = memacpy(struri(base), length);
		if (!uristring) return NULL;

		add_to_strn(&uristring, rel);

		if (translate) {
			unsigned char *translated;

			translated = translate_url(uristring, NULL);
			mem_free(uristring);
			return translated;
		}
		return normalize_uri_reparse(uristring);
	}

	/* Check if there is some protocol name to go for */
	length = get_protocol_length(rel);
	if (length) {
		switch (get_protocol(rel, length)) {
		case PROTOCOL_UNKNOWN:
		case PROTOCOL_PROXY:
			/* Mysteriously proxy URIs are breaking here ... */
			break;

		case PROTOCOL_FILE:
			/* FIXME: Use get_uri_string(base, URI_PATH) as cwd arg
			 * to translate_url(). */
		default:
			uristring = translate_url(rel, NULL);
			if (uristring) return uristring;
		}
	}

	assertm(base->data != NULL, "bad base url");
	if_assert_failed return NULL;

	path = base->data;

	/* Either is path blank, but we've slash char before, or path is not
	 * blank, but doesn't start by a slash (if we'd just stay along with
	 * is_uri_dir_sep(&uri, path[-1]) w/o all the surrounding crap, it
	 * should be enough, but I'm not sure and I don't want to break
	 * anything --pasky). */
	/* We skip first char of URL ('/') in parse_url() (ARGH). This
	 * is reason of all this bug-bearing magic.. */
	if (*path) {
		if (!is_uri_dir_sep(base, *path)) path--;
	} else {
		if (is_uri_dir_sep(base, path[-1])) path--;
	}

	if (!is_uri_dir_sep(base, rel[0])) {
		unsigned char *path_end;

		/* The URL is relative. */

		if (!*path) {
			/* There's no path in the URL, but we're going to add
			 * something there, and the something doesn't start by
			 * a slash. So we need to insert a slash after the base
			 * URL. Clever, eh? ;) */
			add_slash = 1;
		}

		for (path_end = path; *path_end; path_end++) {
			if (end_of_dir(*path_end)) break;
			/* Modify the path pointer, so that it'll always point
			 * above the last '/' in the URL; later, we'll copy the
			 * URL only _TO_ this point, and anything after last
			 * slash will be substituted by 'rel'. */
			if (is_uri_dir_sep(base, *path_end))
				path = path_end + 1;
		}
	}

	length = path - struri(base);
	uristring = mem_alloc(length + strlen(rel) + add_slash + 1);
	if (!uristring) return NULL;

	memcpy(uristring, struri(base), length);
	if (add_slash) uristring[length] = '/';
	strcpy(uristring + length + add_slash, rel);

	return normalize_uri_reparse(uristring);
}


/* Tries to figure out what protocol @newurl might be specifying by checking if
 * it exists as a file locally or by checking parts of the host name. */
static enum protocol
find_uri_protocol(unsigned char *newurl)
{
	unsigned char *ch;

	/* First see if it is a file so filenames that look like hostnames
	 * won't confuse us below. */
	if (check_whether_file_exists(newurl) >= 0) return PROTOCOL_FILE;

	/* Yes, it would be simpler to make test for IPv6 address first,
	 * but it would result in confusing mix of ifdefs ;-). */
	/* FIXME: Ideas for improve protocol detection
	 *
	 * - Handle common hostnames. It could be part of the protocol backend
	 *   structure. [ www -> http, irc -> irc, news -> nntp, ... ]
	 *
	 * - Resolve using port number. [ 119 -> nntp, 443 -> https, ... ]
	 */

	ch = newurl + strcspn(newurl, ".:/@");
	if (*ch == '@'
	    || (*ch == ':' && *newurl != '[' && strchr(newurl, '@'))
	    || !c_strncasecmp(newurl, "ftp.", 4)) {
		/* Contains user/password/ftp-hostname */
		return PROTOCOL_FTP;

#ifdef CONFIG_IPV6
	} else if (*newurl == '[' && *ch == ':') {
		/* Candidate for IPv6 address */
		unsigned char *bracket2, *colon2;

		ch++;
		bracket2 = strchr(ch, ']');
		colon2 = strchr(ch, ':');
		if (bracket2 && colon2 && bracket2 > colon2)
			return PROTOCOL_HTTP;
#endif

	} else if (*newurl != '.' && *ch == '.') {
		/* Contains domain name? */
		unsigned char *host_end, *domain;
		unsigned char *ipscan;

		/* Process the hostname */
		for (domain = ch + 1;
			*(host_end = domain + strcspn(domain, ".:/?")) == '.';
			domain = host_end + 1);

		/* It's IP? */
		for (ipscan = ch; isdigit(*ipscan) || *ipscan == '.';
			ipscan++);

		if (!*ipscan || *ipscan == ':' || *ipscan == '/')
			return PROTOCOL_HTTP;

		/* It's two-letter or known TLD? */
		if (host_end - domain == 2
		    || end_with_known_tld(domain, host_end - domain) >= 0)
			return PROTOCOL_HTTP;
	}

	return PROTOCOL_UNKNOWN;
}


#define MAX_TRANSLATION_ATTEMPTS	32

/* Returns an URI string that can be used internally. Adding protocol prefix,
 * missing slashes etc. */
static unsigned char *
translate_url(unsigned char *url, unsigned char *cwd)
{
	unsigned char *newurl;
	struct uri uri;
	enum uri_errno uri_errno, prev_errno = URI_ERRNO_EMPTY;
	int retries = 0;

	/* Strip starting spaces */
	while (*url == ' ') url++;
	if (!*url) return NULL;

	newurl = expand_tilde(url); /* XXX: Post data copy. */
	if (!newurl) return NULL;

parse_uri:
	/* Yay a goto loop. If we get some URI parse error and try to
	 * fix it we go back to here and try again. */
	/* Ordinary parse */
	uri_errno = parse_uri(&uri, newurl);

	/* Bail out if the same error occurs twice */
	if (uri_errno == prev_errno || retries++ > MAX_TRANSLATION_ATTEMPTS) {
		if (retries > MAX_TRANSLATION_ATTEMPTS) {
			ERROR("Maximum number of parsing attempts exceeded "
			      "for %s.", url);
		}
		mem_free(newurl);
		return NULL;
	}

	prev_errno = uri_errno;

	switch (uri_errno) {
	case URI_ERRNO_OK:
		/* Fix translation of 1.2.3.4:5 so IP address part won't be
		 * interpreted as the protocol name. */
		if (uri.protocol == PROTOCOL_UNKNOWN) {
			enum protocol protocol = find_uri_protocol(newurl);

			/* Code duplication with the URI_ERRNO_INVALID_PROTOCOL
			 * case. */
			if (protocol != PROTOCOL_UNKNOWN) {
				struct string str;

				if (!init_string(&str)) return NULL;

				switch (protocol) {
				case PROTOCOL_FTP:
					add_to_string(&str, "ftp://");
					encode_uri_string(&str, newurl, -1, 0);
					break;

				case PROTOCOL_HTTP:
					add_to_string(&str, "http://");
					add_to_string(&str, newurl);
					break;

				case PROTOCOL_UNKNOWN:
					break;

				case PROTOCOL_FILE:
				default:
					add_to_string(&str, "file://");
					if (!dir_sep(*newurl))
						add_to_string(&str, "./");

					add_to_string(&str, newurl);
				}

				mem_free(newurl);
				newurl = str.source;

				/* Work around the infinite loop prevention */
				prev_errno = URI_ERRNO_EMPTY;
				goto parse_uri;
			}
		}

		/* If file:// URI is transformed we need to reparse. */
		if (uri.protocol == PROTOCOL_FILE && cwd && *cwd
		    && transform_file_url(&uri, cwd))
			return normalize_uri_reparse(struri(&uri));

		/* Translate the proxied URI too if proxy:// */
		if (uri.protocol == PROTOCOL_PROXY) {
			unsigned char *data = translate_url(uri.data, cwd);
			int pos = uri.data - struri(&uri);

			if (!data) break;
			struri(&uri)[pos] = 0;
			insert_in_string(&struri(&uri), pos, data, strlen(data));
			mem_free(data);
			return normalize_uri_reparse(struri(&uri));
		}

		return normalize_uri_noparse(&uri);

	case URI_ERRNO_TOO_MANY_SLASHES:
	{
		unsigned char *from, *to;

		assert(uri.string[uri.protocollen] == ':'
		       && uri.string[uri.protocollen + 1] == '/'
		       && uri.string[uri.protocollen + 2] == '/');

		from = to = uri.string + uri.protocollen + 3;
		while (*from == '/') from++;

		assert(to < from);
		memmove(to, from, strlen(from) + 1);
		goto parse_uri;
	}
	case URI_ERRNO_NO_SLASHES:
	{
		/* Try prefix:some.url -> prefix://some.url.. */
		int slashes = 2;

		/* Check if only one '/' is needed. */
		if (uri.string[uri.protocollen + 1] == '/')
			slashes--;

		insert_in_string(&newurl, uri.protocollen + 1, "//", slashes);
		goto parse_uri;
	}
	case URI_ERRNO_TRAILING_DOTS:
	{
		/* Trim trailing '.'s */
		unsigned char *from = uri.host + uri.hostlen;
		unsigned char *to = from;

		assert(uri.host < to && to[-1] == '.' && *from != '.');

		while (uri.host < to && to[-1] == '.') to--;

		assert(to < from);
		memmove(to, from, strlen(from) + 1);
		goto parse_uri;
	}
	case URI_ERRNO_NO_PORT_COLON:
		assert(uri.portlen == 0
		       && uri.string < uri.port
		       && uri.port[-1] == ':');

		memmove(uri.port - 1, uri.port, strlen(uri.port) + 1);
		goto parse_uri;

	case URI_ERRNO_NO_HOST_SLASH:
	{
		int offset = uri.port
			   ? uri.port + uri.portlen - struri(&uri)
			   : uri.host + uri.hostlen - struri(&uri) + uri.ipv6 /* ']' */;

		assertm(uri.host != NULL, "uri.host not set after no host slash error");
		insert_in_string(&newurl, offset, "/", 1);
		goto parse_uri;
	}
	case URI_ERRNO_INVALID_PROTOCOL:
	{
		/* No protocol name */
		enum protocol protocol = find_uri_protocol(newurl);
		struct string str;

		if (!init_string(&str)) return NULL;

		switch (protocol) {
			case PROTOCOL_FTP:
				add_to_string(&str, "ftp://");
				encode_uri_string(&str, newurl, -1, 0);
				break;

			case PROTOCOL_HTTP:
				add_to_string(&str, "http://");
				add_to_string(&str, newurl);
				break;

			case PROTOCOL_UNKNOWN:
				/* We default to file:// even though we already
				 * tested if the file existed since it will give
				 * a "No such file or directory" error.  which
				 * might better hint the user that there was
				 * problem figuring out the URI. */
			case PROTOCOL_FILE:
			default:
				add_to_string(&str, "file://");
				if (!dir_sep(*newurl))
					add_to_string(&str, "./");

				encode_file_uri_string(&str, newurl);
		}

		mem_free(newurl);
		newurl = str.source;

		goto parse_uri;
	}
	case URI_ERRNO_EMPTY:
	case URI_ERRNO_IPV6_SECURITY:
	case URI_ERRNO_NO_HOST:
	case URI_ERRNO_INVALID_PORT:
	case URI_ERRNO_INVALID_PORT_RANGE:
		/* None of these can be handled properly. */
		break;
	}

	mem_free(newurl);
	return NULL;
}


struct uri *
get_composed_uri(struct uri *uri, enum uri_component components)
{
	unsigned char *string;

	assert(uri);
	if_assert_failed return NULL;

	string = get_uri_string(uri, components);
	if (!string) return NULL;

	uri = get_uri(string, 0);
	mem_free(string);

	return uri;
}

struct uri *
get_translated_uri(unsigned char *uristring, unsigned char *cwd)
{
	struct uri *uri;

	uristring = translate_url(uristring, cwd);
	if (!uristring) return NULL;

	uri = get_uri(uristring, 0);
	mem_free(uristring);

	return uri;
}


unsigned char *
get_extension_from_uri(struct uri *uri)
{
	unsigned char *extension = NULL;
	int afterslash = 1;
	unsigned char *pos = uri->data;

	assert(pos);

	for (; *pos && !end_of_dir(*pos); pos++) {
		if (!afterslash && !extension && *pos == '.') {
			extension = pos;
		} else if (is_uri_dir_sep(uri, *pos)) {
			extension = NULL;
			afterslash = 1;
		} else {
			afterslash = 0;
		}
	}

	if (extension && extension < pos)
		return memacpy(extension, pos - extension);

	return NULL;
}

/* URI encoding, escaping unallowed characters. */
static inline int
safe_char(unsigned char c)
{
	/* RFC 2396, Page 8, Section 2.3 ;-) */
	return isident(c) || c == '.' || c == '!' || c == '~'
	       || c == '*' || c == '\''|| c == '(' || c == ')';
}

void
encode_uri_string(struct string *string, const unsigned char *name, int namelen,
		  int convert_slashes)
{
	unsigned char n[4];
	const unsigned char *end;

	n[0] = '%';
	n[3] = '\0';

	if (namelen < 0) namelen = strlen(name);

	for (end = name + namelen; name < end; name++) {
#if 0
		/* This is probably correct only for query part of URI..? */
		if (*name == ' ') add_char_to_string(data, len, '+');
		else
#endif
		if (safe_char(*name) || (!convert_slashes && *name == '/')) {
			add_char_to_string(string, *name);
		} else {
			/* Hex it. */
			n[1] = hx((((int) *name) & 0xF0) >> 4);
			n[2] = hx(((int) *name) & 0xF);
			add_bytes_to_string(string, n, sizeof(n) - 1);
		}
	}
}

void
encode_win32_uri_string(struct string *string, unsigned char *name, int namelen)
{
	unsigned char n[4];
	unsigned char *end;

	n[0] = '%';
	n[3] = '\0';

	if (namelen < 0) namelen = strlen(name);

	for (end = name + namelen; name < end; name++) {
		if (safe_char(*name) || *name == ':' || *name == '\\') {
			add_char_to_string(string, *name);
		} else {
			/* Hex it. */
			n[1] = hx((((int) *name) & 0xF0) >> 4);
			n[2] = hx(((int) *name) & 0xF);
			add_bytes_to_string(string, n, sizeof(n) - 1);
		}
	}
}

/* This function is evil, it modifies its parameter. */
/* XXX: but decoded string is _never_ longer than encoded string so it's an
 * efficient way to do that, imho. --Zas */
void
decode_uri(unsigned char *src)
{
	unsigned char *dst = src;
	unsigned char c;

	do {
		c = *src++;

		if (c == '%') {
			int x1 = unhx(*src);

			if (x1 >= 0) {
				int x2 = unhx(*(src + 1));

				if (x2 >= 0) {
					x1 = (x1 << 4) + x2;
					if (x1 != 0) { /* don't allow %00 */
						c = (unsigned char) x1;
						src += 2;
					}
				}
			}

#if 0
		} else if (c == '+') {
			/* As the comment in encode_uri_string suggests, '+'
			 * should only be decoded in the query part of a URI
			 * (should that be 'URL'?). I'm not bold enough to
			 * disable this code, tho. -- Miciah */
			c = ' ';
#endif
		}

		*dst++ = c;
	} while (c != '\0');
}

void
decode_uri_string(struct string *string)
{
	decode_uri(string->source);
	string->length = strlen(string->source);
}

void
decode_uri_for_display(unsigned char *src)
{
	decode_uri(src);

	for (; *src; src++)
		if (!isprint(*src) || iscntrl(*src))
			*src = '*';
}

void
decode_uri_string_for_display(struct string *string)
{
	decode_uri_for_display(string->source);
	string->length = strlen(string->source);
}


/* URI list */

#define URI_LIST_GRANULARITY 0x3

#define realloc_uri_list(list) \
	mem_align_alloc(&(list)->uris, (list)->size, (list)->size + 1, \
			URI_LIST_GRANULARITY)

struct uri *
add_to_uri_list(struct uri_list *list, struct uri *uri)
{
	if (!realloc_uri_list(list))
		return NULL;

	list->uris[list->size++] = get_uri_reference(uri);

	return uri;
};

void
free_uri_list(struct uri_list *list)
{
	struct uri *uri;
	int index;

	if (!list->uris) return;

	foreach_uri (uri, index, list) {
		done_uri(uri);
	}

	mem_free_set(&list->uris, NULL);
	list->size = 0;
}

/* URI cache */

struct uri_cache_entry {
	struct uri uri;
	unsigned char string[1];
};

struct uri_cache {
	struct hash *map;
	struct object object;
};

static struct uri_cache uri_cache;

#ifdef CONFIG_DEBUG
static inline void
check_uri_sanity(struct uri *uri)
{
	int pos;

	for (pos = 0; pos < uri->protocollen; pos++)
		if (c_isupper(uri->string[pos])) goto error;

	if (uri->hostlen)
		for (pos = 0; pos < uri->hostlen; pos++)
			if (c_isupper(uri->host[pos])) goto error;
	return;
error:
	INTERNAL("Uppercase letters detected in protocol or host part (%s).", struri(uri));
}
#else
#define check_uri_sanity(uri)
#endif

static inline struct uri_cache_entry *
get_uri_cache_entry(unsigned char *string, int length)
{
	struct uri_cache_entry *entry;
	struct hash_item *item;

	assert(string && length > 0);
	if_assert_failed return NULL;

	item = get_hash_item(uri_cache.map, string, length);
	if (item) return item->value;

	/* Setup a new entry */

	entry = mem_calloc(1, sizeof(*entry) + length);
	if (!entry) return NULL;

	object_nolock(&entry->uri, "uri");
	memcpy(&entry->string, string, length);
	string = entry->string;

	if (parse_uri(&entry->uri, string) != URI_ERRNO_OK
	    || !add_hash_item(uri_cache.map, string, length, entry)) {
		mem_free(entry);
		return NULL;
	}

	object_lock(&uri_cache);

	return entry;
}

struct uri *
get_uri(unsigned char *string, enum uri_component components)
{
	struct uri_cache_entry *entry;

	assert(string);

	if (components) {
		struct uri uri;

		if (parse_uri(&uri, string) != URI_ERRNO_OK)
			return NULL;

		return get_composed_uri(&uri, components);
	}

	if (!is_object_used(&uri_cache)) {
		uri_cache.map = init_hash8();
		if (!uri_cache.map) return NULL;
		object_nolock(&uri_cache, "uri_cache");
	}

	entry = get_uri_cache_entry(string, strlen(string));
	if (!entry) {
		if (!is_object_used(&uri_cache))
			free_hash(&uri_cache.map);
		return NULL;
	}

	check_uri_sanity(&entry->uri);
	object_nolock(&entry->uri, "uri");
	object_lock(&entry->uri);

	return &entry->uri;
}

void
done_uri(struct uri *uri)
{
	unsigned char *string = struri(uri);
	int length = strlen(string);
	struct hash_item *item;
	struct uri_cache_entry *entry;

	assert(is_object_used(&uri_cache));

	object_unlock(uri);
	if (is_object_used(uri)) return;

	item = get_hash_item(uri_cache.map, string, length);
	entry = item ? item->value : NULL;

	assertm(entry != NULL, "Releasing unknown URI [%s]", string);
	del_hash_item(uri_cache.map, item);
	mem_free(entry);

	/* Last URI frees the cache */
	object_unlock(&uri_cache);
	if (!is_object_used(&uri_cache))
		free_hash(&uri_cache.map);
}
