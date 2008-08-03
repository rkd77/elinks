/* Shared protocol functions */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* FreeBSD needs this before resource.h */
#endif
#include <sys/types.h> /* FreeBSD needs this before resource.h */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "osdep/osdep.h"
#include "protocol/common.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Close all non-terminal file descriptors. */
void
close_all_non_term_fd(void)
{
	int n;
	int max = 1024;
#ifdef RLIMIT_NOFILE
	struct rlimit lim;

	if (!getrlimit(RLIMIT_NOFILE, &lim))
		max = lim.rlim_max;
#endif
	for (n = 3; n < max; n++)
		close(n);
}

struct connection_state
init_directory_listing(struct string *page, struct uri *uri)
{
	struct string dirpath = NULL_STRING;
	struct string decoded = NULL_STRING;
	struct string location = NULL_STRING;
	unsigned char *info;
	int local = (uri->protocol == PROTOCOL_FILE);

	if (!init_string(page)
	    || !init_string(&dirpath)
	    || !init_string(&decoded)
	    || !init_string(&location)
	    || !add_uri_to_string(&dirpath, uri, URI_DATA)
	    || !add_uri_to_string(&location, uri, URI_DIR_LOCATION))
		goto out_of_memory;

	if (dirpath.length > 0
	    && !dir_sep(dirpath.source[dirpath.length - 1])
	    && !add_char_to_string(&dirpath, local ? CHAR_DIR_SEP : '/'))
		goto out_of_memory;

	/* Decode uri for displaying.  */
	if (!add_string_to_string(&decoded, &dirpath))
		goto out_of_memory;
	decode_uri_string(&decoded);

	if (!local && !add_char_to_string(&location, '/'))
		goto out_of_memory;

	if (!add_to_string(page, "<html>\n<head><title>"))
		goto out_of_memory;

	if (!local && !add_html_to_string(page, location.source, location.length))
		goto out_of_memory;

	if (!add_html_to_string(page, decoded.source, decoded.length)
	    || !add_to_string(page, "</title>\n<base href=\"")
	    || !add_html_to_string(page, location.source, location.length)
	    || !add_html_to_string(page, dirpath.source, dirpath.length))
		goto out_of_memory;

	if (!add_to_string(page, "\" />\n</head>\n<body>\n<h2>"))
		goto out_of_memory;

	/* Use module names? */
	switch (uri->protocol) {
	case PROTOCOL_FILE:
		info = "Local";
		break;
	case PROTOCOL_FSP:
		info = "FSP";
		break;
	case PROTOCOL_FTP:
		info = "FTP";
		break;
	case PROTOCOL_GOPHER:
		info = "Gopher";
		break;
	case PROTOCOL_SMB:
		info = "Samba";
		break;
	default:
		info = "?";
	}

	if (!add_to_string(page, info)
	    || !add_to_string(page, " directory "))
		goto out_of_memory;

	if (!local && !add_string_to_string(page, &location))
		goto out_of_memory;

	/* Make the directory path with links to each subdir. */
	{
		const unsigned char *slash = dirpath.source;
		const unsigned char *pslash = slash;
		const unsigned char sep = local ? CHAR_DIR_SEP :  '/';

		while ((slash = strchr(slash, sep)) != NULL) {
			done_string(&decoded);
			if (!init_string(&decoded)
			    || !add_bytes_to_string(&decoded, pslash, slash - pslash))
				goto out_of_memory;
			decode_uri_string(&decoded);

			if (!add_to_string(page, "<a href=\"")
			    || !add_html_to_string(page, location.source, location.length)
			    || !add_html_to_string(page, dirpath.source, slash + 1 - dirpath.source)
			    || !add_to_string(page, "\">")
			    || !add_html_to_string(page, decoded.source, decoded.length)
			    || !add_to_string(page, "</a>")
			    || !add_html_to_string(page, &sep, 1))
				goto out_of_memory;

			pslash = ++slash;
		}
	}

	if (!add_to_string(page, "</h2>\n<pre>")) {
out_of_memory:
		done_string(page);
	}

	done_string(&dirpath);
	done_string(&decoded);
	done_string(&location);

	return page->length > 0
		? connection_state(S_OK)
		: connection_state(S_OUT_OF_MEM);
}
