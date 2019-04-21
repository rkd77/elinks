#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

#include "main/object.h"

struct string;

#define POST_CHAR 1
#define POST_CHAR_S "\001"
#define FILE_CHAR '\002'

/* The uri structure is used to store the start position and length of commonly
 * used uri fields. It is initialized by parse_uri(). It is possible that the
 * start of a field is set but that the length is zero so instead of testing
 * *uri-><fieldname> always use uri-><fieldname>len. */
/* XXX: Lots of places in the code assume that the string members point into
 * the same string. That means if you need to use a NUL terminated uri field
 * either temporary modify the string, allocated a copy or change the function
 * used to take a length parameter (in the reverse precedence - modifying the
 * string should not be done since you never know what kind of memory actually
 * contains the string --pasky). */
/* TODO: We should probably add path+query members instead of data. */

struct uri {
	/** The start of the URI (and thus start of the protocol %string).
	 * The format of the whole %string is like:
	 * "http6://elinks.cz/dir/file?query#frag" ::POST_CHAR post_data "\0"
	 *
	 * The post_data is not really %part of the URI but ELinks keeps it
	 * in the same %string and can then distinguish between cache entries
	 * for different POST requests.  See uri.post for its syntax.  */
	unsigned char *string;

	/* The internal type of protocol. Can _never_ be PROTOCOL_UNKNOWN. */
	int protocol; /* enum protocol */

	/* A special ELinks extension allows i.e. 'http4' or 'ftp6' protocols,
	 * forcing the given IP family. 0 means the IP family is not forced. */
	int ip_family;

	unsigned char *user;
	unsigned char *password;
	unsigned char *host;
	unsigned char *port;
	/* @data can contain both the path and query uri fields.
	 * It can never be NULL but can have zero length. */
	unsigned char *data;
	unsigned char *fragment;

	/** POST data attached to the URI.  If uri.string contains a
	 * ::POST_CHAR, then @c post points to the following
	 * character.  Otherwise NULL.  The syntax of the POST data
	 * is:
	 *
	 * [content-type '\\n']
	 * (hexadecimal-byte | ::FILE_CHAR file-name ::FILE_CHAR)*
	 *
	 * - If content-type is present, ELinks sends "Content-Type: ",
	 *   content-type, and CRLF in the head of the POST request.
	 *
	 * - Each hexadecimal-byte is a byte for the body of the POST
	 *   request.  It is encoded as two lower-case hexadecimal
	 *   digits, most significant first.  For example, "0a" for
	 *   ::ASCII_LF.
	 *
	 * - file-name is the name of a file that ELinks should send
	 *   to the server.  It is in the charset accepted by open(),
	 *   and some characters (especially ::FILE_CHAR) are
	 *   percent-encoded.  */
	unsigned char *post;

	/* @protocollen should only be usable if @protocol is either
	 * PROTOCOL_USER or an uri string should be composed. */
	unsigned int protocollen:16;
	unsigned int userlen:16;
	unsigned int passwordlen:16;
	unsigned int hostlen:16;
	unsigned int portlen:8;
	unsigned int datalen:16;
	unsigned int fragmentlen:16;

	/* Flags */
	unsigned int ipv6:1;	/* URI contains IPv6 host */
	unsigned int form:1;	/* URI originated from form */

	/* Usage count object. */
	struct object object;
};

enum uri_errno {
	URI_ERRNO_OK,			/* Parsing went well */
	URI_ERRNO_EMPTY,		/* The URI string was empty */
	URI_ERRNO_INVALID_PROTOCOL,	/* No protocol was found */
	URI_ERRNO_NO_SLASHES,		/* Slashes after protocol missing */
	URI_ERRNO_TOO_MANY_SLASHES,	/* Too many slashes after protocol */
	URI_ERRNO_TRAILING_DOTS,	/* '.' after host */
	URI_ERRNO_NO_HOST,		/* Host part is missing */
	URI_ERRNO_NO_PORT_COLON,	/* ':' after host without port */
	URI_ERRNO_NO_HOST_SLASH,	/* Slash after host missing */
	URI_ERRNO_IPV6_SECURITY,	/* IPv6 security bug detected */
	URI_ERRNO_INVALID_PORT,		/* Port number is bad */
	URI_ERRNO_INVALID_PORT_RANGE,	/* Port number is not within 0-65535 */
};

/* Initializes the members of the uri struct, as they are encountered.
 * If an uri component is recognized both it's length and starting point is
 * set. */
/* Returns what error was encountered or URI_ERRNO_OK if parsing went well. */
enum uri_errno parse_uri(struct uri *uri, unsigned char *uristring);


/* Returns the raw zero-terminated URI string the (struct uri) is associated
 * with. Thus, chances are high that it is the original URI received, not any
 * cheap reconstruction. */
#define struri(uri) ((uri)->string)


enum uri_component {
	/**** The "raw" URI components */

	URI_PROTOCOL		= (1 << 0),
	URI_IP_FAMILY		= (1 << 1),
	URI_USER		= (1 << 2),
	URI_PASSWORD		= (1 << 3),
	URI_HOST		= (1 << 4),
	URI_PORT		= (1 << 5),
	URI_DEFAULT_PORT	= (1 << 6),
	URI_DATA		= (1 << 7),
	URI_FRAGMENT		= (1 << 8),
	URI_POST		= (1 << 9),
	URI_POST_INFO		= (1 << 10),


	/**** Flags affecting appearance of the components above, or special
	 * mutations of mixups of some of the raw components. */

	/* Control for ``encoding'' URIs into Internationalized Domain Names.
	 * Hopefully only a few lowlevel places should have to use it and it
	 * should never be exposed to the user. */
	URI_IDN			= (1 << 11),

	/* Add stuff from uri->data and up and prefixes a '/' */
	URI_PATH		= (1 << 12),

	/* Add filename from last direcory separator in uri->data to end of
	 * path. */
	URI_FILENAME		= (1 << 13),

	/* Add query part from uri->data not including the '?' */
	URI_QUERY		= (1 << 14),


	/**** Some predefined classes for formatting of URIs */

	/* Special flags */
	URI_SPECIAL		= URI_DEFAULT_PORT | URI_PATH | URI_FILENAME | URI_QUERY,

	/* The usual suspects */
	URI_RARE		= URI_SPECIAL | URI_POST | URI_POST_INFO | URI_IDN,

	/* Used _only_ for displaying URIs in dialogs or document titles. */
	URI_PUBLIC		= ~(URI_PASSWORD | URI_RARE) | URI_POST_INFO,

	/* Used for getting the original URI with no internal post encoding */
	URI_ORIGINAL		= ~URI_RARE,

	/* Used for getting the URI with no #fragment */
	URI_BASE		= ~(URI_RARE | URI_FRAGMENT) | URI_POST,

	/* Used for getting data-less URI (stuff only up to the slash). */
	URI_SERVER		= ~(URI_RARE | URI_DATA | URI_FRAGMENT),

	/* Used in the HTTP Auth code */
	URI_HTTP_AUTH		= ~(URI_RARE | URI_USER | URI_PASSWORD | URI_DATA | URI_FRAGMENT),

	/* Used for the value of HTTP "Host" header info */
	URI_HTTP_HOST		= URI_HOST | URI_PORT | URI_IDN,

	/* Used for the host part of HTTP referrer. Stripped from user info. */
	URI_HTTP_REFERRER_HOST	= URI_PROTOCOL | URI_HOST | URI_PORT,

	/* Used for the whole HTTP referrer. Contains no user/passwd info. */
	URI_HTTP_REFERRER	= URI_HTTP_REFERRER_HOST | URI_DATA,

	/* Used for HTTP CONNECT method info */
	URI_HTTP_CONNECT	= URI_HOST | URI_PORT | URI_DEFAULT_PORT,

	/* Used for adding directory listing HTML header, */
	URI_DIR_LOCATION	= URI_PROTOCOL | URI_HOST | URI_PORT | URI_IDN,

	/* Used for getting the host of a DNS query. As a hidden bonus we get
	 * IPv6 hostnames without the brackets because we don't ask for
	 * URI_PORT. */
	URI_DNS_HOST		= URI_HOST | URI_IDN,

	/* Used for adding the unproxied URI and encode it using IDN to string */
	URI_PROXY		= ~(URI_RARE | URI_FRAGMENT) | URI_IDN,

	/* Used for comparing keepalive connection URIs */
	/* (We don't need to bother by explicit IP family, we don't care
	 * whether the actual query goes over IPv4 or IPv6 but only about
	 * new connections. Of course another thing is what the user expects
	 * us to care about... ;-) --pasky */
	URI_KEEPALIVE		= URI_PROTOCOL | URI_USER | URI_PASSWORD | URI_HOST | URI_PORT,

	/* Used for the form action URI using the GET method */
	URI_FORM_GET		= URI_SERVER | URI_PATH,
};


/* List for maintaining multiple URIs. Free it with mem_free() */
struct uri_list {
	int size;
	struct uri **uris;
};

#define foreach_uri(uri, index, list) \
	for (index = 0; index < (list)->size; index++) \
		if ((uri = (list)->uris[index]))

/* Adds @uri to the URI list */
struct uri *add_to_uri_list(struct uri_list *list, struct uri *uri);

/* Free all entries in the URI list */
void free_uri_list(struct uri_list *list);


/* A small URI struct cache to increase reusability. */
/* XXX: Now there are a few rules to abide.
 *
 * Any URI string that should be registered in the cache has to have lowercased
 * both the protocol and hostname parts. This is strictly checked and will
 * otherwise cause an assertion failure.
 *
 * However this will not be a problem if you either first call join_urls()
 * which you want to do anyway to resolve relative references or use the
 * get_translated_uri() interface.
 *
 * The remaining support for RFC 2396 section 3.1 is done through get_protocol()
 * and get_user_program() which will treat upper case letters
 * as equivalent to lower case in protocol names. */

/* Register a new URI in the cache where @components controls which parts are
 * added to the returned URI. */
struct uri *get_uri(unsigned char *string, enum uri_component components);

/* Dereference an URI from the cache */
void done_uri(struct uri *uri);

/* Take a reference of an URI already registered in the cache. */
static inline struct uri *
get_uri_reference(struct uri *uri)
{
	object_lock(uri);
	return uri;
}

/* Get URI using the string returned by get_uri_string(@uri, @components) */
struct uri *get_composed_uri(struct uri *uri, enum uri_component components);

/* Resolves an URI relative to a current working directory (CWD) and possibly
 * extracts the fragment. It is possible to just use it to extract fragment
 * and get the resulting URI from the cache.
 * @uristring	is the URI to resolve or translate.
 * @cwd		if non NULL @uristring will be translated using this CWD. */
struct uri *get_translated_uri(unsigned char *uristring, unsigned char *cwd);

/* Normalizes the directory structure given in uristring. XXX: The function
 * modifies the uristring and returns it. The uri argument should be NULL
 * if the uri is not the parsed uristring. */
unsigned char *normalize_uri(struct uri *uri, unsigned char *uristring);

/* Check if two URIs are equal. If @components are 0 simply compare the whole
 * URI else only compare the specific parts. */
int compare_uri(const struct uri *uri1, const struct uri *uri2,
		enum uri_component components);

/* These functions recreate the URI string part by part. */
/* The @components bitmask describes the set of URI components used for
 * construction of the URI string. */

/* Adds the components to an already initialized string. */
struct string *add_uri_to_string(struct string *string, const struct uri *uri,
				 enum uri_component components);

/* Takes an uri string, parses it and adds the desired components. Useful if
 * there is no struct uri around. */
struct string *add_string_uri_to_string(struct string *string, unsigned char *uristring, enum uri_component components);

/* Returns the new URI string or NULL upon an error. */
unsigned char *get_uri_string(const struct uri *uri,
			      enum uri_component components);

/* Returns either the uri's port number if available or the protocol's
 * default port. It is zarro for user protocols. */
int get_uri_port(const struct uri *uri);

/* Tcp port range */
#define LOWEST_PORT	0
#define HIGHEST_PORT	65535

#define uri_port_is_valid(port) \
	(LOWEST_PORT <= (port) && (port) <= HIGHEST_PORT)


/* Encode and add @namelen bytes from @name to @string. If @namelen is -1 it is
 * set to strlen(@name). If the boolean convert_slashes is zero '/'-chars will
 * not be encoded. */
void encode_uri_string(struct string *string, const unsigned char *name, int namelen,
		       int convert_slashes);

/* special version for Windows directory listing */
void encode_win32_uri_string(struct string *string, unsigned char *name, int namelen);

void decode_uri_string(struct string *string);
void decode_uri(unsigned char *uristring);

/* Decodes and replaces illicit screen chars with '*'. */
void decode_uri_string_for_display(struct string *string);
void decode_uri_for_display(unsigned char *uristring);

/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_uri(struct uri *uri);


/* Resolves a @relative URI to absolute form using @base URI.
 * Example: if @base is http://elinks.cz/ and @relative is #news
 *	    the outcome would be http://elinks.cz/#news */
unsigned char *join_urls(struct uri *base, unsigned char *relative);

/* Return position if end of string @s matches a known tld or -1 if not.
 * If @slen < 0, then string length will be obtained by a strlen() call,
 * else @slen is used as @s length. */
int end_with_known_tld(const unsigned char *s, int slen);


static inline int
get_real_uri_length(struct uri *uri)
{
	return uri->post ? uri->post - struri(uri) - 1 : strlen(struri(uri));
}

/* Checks if @address contains a valid IP address. */
int is_ip_address(const unsigned char *address, int addresslen);

/* Check whether domain is matching server
 * Ie.
 * example.com matches www.example.com/
 * example.com doesn't match www.example.com.org/
 * example.com doesn't match www.example.comm/
 * example.com doesn't match example.co
 */
int is_in_domain(unsigned char *domain, unsigned char *server, int server_len);

#endif
