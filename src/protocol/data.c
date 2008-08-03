/* The "data" URI protocol implementation (RFC 2397) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "network/connection.h"
#include "protocol/data.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/base64.h"
#include "util/string.h"

/* The URLs are of the form:
 *
 *	data:[<mediatype>][;base64],<data>
 *
 * The <mediatype> is an Internet media type specification (with optional
 * parameters.) The appearance of ";base64" means that the data is encoded as
 * base64. Without ";base64", the data (as a sequence of octets) is represented
 * using ASCII encoding for octets inside the range of safe URL characters and
 * using the standard %xx hex encoding of URLs for octets outside that range.
 * If <mediatype> is omitted, it defaults to "text/plain;charset=US-ASCII".  As a
 * shorthand, "text/plain" can be omitted but the charset parameter supplied.
 *
 * The syntax:
 *
 *	dataurl	  := "data:" [ mediatype ] [ ";base64" ] "," data
 *	mediatype := [ type "/" subtype ] *( ";" parameter )
 *	data	  := *urlchar
 *	parameter := attribute "=" value
 *
 * where "urlchar" is imported from [RFC2396], and "type", "subtype",
 * "attribute" and "value" are the corresponding tokens from [RFC2045],
 * represented using URL escaped encoding of [RFC2396] as necessary.
 *
 * Attribute values in [RFC2045] are allowed to be either represented as tokens
 * or as quoted strings. However, within a "data" URL, the "quoted-string"
 * representation would be awkward, since the quote mark is itself not a valid
 * urlchar. For this reason, parameter values should use the URL Escaped
 * encoding instead of quoted string if the parameter values contain any
 * "tspecial".
 *
 * The ";base64" extension is distinguishable from a content-type parameter by
 * the fact that it doesn't have a following "=" sign. */

/* FIXME: Maybe some kind of redirecting to common specialized data URI could
 * be useful so "data:,blah" and data:text/plain,blah" are redirected to the
 * most specialized "data:text/plain;charset=US-ASCII,blah". On the other hand
 * for small entries it doesn't matter. */

#define	DEFAULT_DATA_MEDIATYPE	"text/plain;charset=US-ASCII"

#define data_has_mediatype(header, headerlen) \
	((headerlen) >= 3 && memchr(header, '/', headerlen))

#define data_has_base64_attribute(typelen, endstr) \
	((typelen) >= sizeof(";base64") - 1 \
	 && !memcmp(";base64", (end) - sizeof(";base64") + 1, sizeof(";base64") - 1))

static unsigned char *
init_data_protocol_header(struct cache_entry *cached,
			  unsigned char *type, int typelen)
{
	unsigned char *head;

	assert(typelen);

	type = memacpy(type, typelen);
	if (!type) return NULL;

	/* Set fake content type */
	head = straconcat("\r\nContent-Type: ", type, "\r\n",
			  (unsigned char *) NULL);
	mem_free(type);
	if (!head) return NULL;

	mem_free_set(&cached->head, head);
	return head;
}

static unsigned char *
parse_data_protocol_header(struct connection *conn, int *base64)
{
	struct uri *uri = conn->uri;
	unsigned char *end = memchr(uri->data, ',', uri->datalen);
	unsigned char *type = DEFAULT_DATA_MEDIATYPE;
	int typelen = sizeof(DEFAULT_DATA_MEDIATYPE) - 1;

	if (end) {
		int headerlen = end - uri->data;

		if (data_has_base64_attribute(headerlen, end)) {
			*base64 = 1;
			headerlen -= sizeof(";base64") - 1;
		}

		if (data_has_mediatype(uri->data, headerlen)) {
			type	= uri->data;
			typelen	= headerlen;
		}
	}

	if (!init_data_protocol_header(conn->cached, type, typelen))
		return NULL;

	/* Return char after ',' or complete data part */
	return end ? end + 1 : uri->data;
}

void
data_protocol_handler(struct connection *conn)
{
	struct uri *uri = conn->uri;
	struct cache_entry *cached = get_cache_entry(uri);
	unsigned char *data_start, *data;
	int base64 = 0;

	if (!cached) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	conn->cached = cached;

	data_start = parse_data_protocol_header(conn, &base64);
	if (!data_start) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	/* Allocate the data string because URI decoding will possibly modify
	 * it. */
	data = memacpy(data_start, uri->datalen - (data_start - uri->data));
	if (!data) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (base64) {
		unsigned char *decoded = base64_encode(data);

		if (!decoded) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}

		mem_free_set(&data, decoded);
	} else {
		decode_uri(data);
	}

	{
		/* Use strlen() to get the correct decoded length */
		int datalen = strlen(data);

		add_fragment(cached, conn->from, data, datalen);
		conn->from += datalen;
	}

	mem_free(data);

	abort_connection(conn, connection_state(S_OK));
}
