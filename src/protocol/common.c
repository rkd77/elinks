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

enum connection_state
init_directory_listing(struct string *page, struct uri *uri)
{
	struct string dirpath = NULL_STRING;
	struct string location = NULL_STRING;
	unsigned char *info;
	int local = (uri->protocol == PROTOCOL_FILE);

	if (!init_string(page)
	    || !init_string(&dirpath)
	    || !init_string(&location)
	    || !add_uri_to_string(&dirpath, uri, URI_DATA)
	    || !add_uri_to_string(&location, uri, URI_DIR_LOCATION))
		goto out_of_memory;

	if (dirpath.length > 0
	    && !dir_sep(dirpath.source[dirpath.length - 1])
	    && !add_char_to_string(&dirpath, local ? CHAR_DIR_SEP : '/'))
		goto out_of_memory;

	if (local || uri->protocol == PROTOCOL_FSP || uri->protocol == PROTOCOL_GOPHER
		  || uri->protocol == PROTOCOL_SMB) {
		/* A little hack to get readable Gopher names. We should find a
		 * way to do it more general. */
		decode_uri_string(&dirpath);
	}

	if (!local && !add_char_to_string(&location, '/'))
		goto out_of_memory;

	if (!add_to_string(page, "<html>\n<head><title>"))
		goto out_of_memory;

	if (!local && !add_string_to_string(page, &location))
		goto out_of_memory;

	if (!add_html_to_string(page, dirpath.source, dirpath.length)
	    || !add_to_string(page, "</title>\n<base href=\"")
	    || !add_string_to_string(page, &location))
		goto out_of_memory;

#ifdef CONFIG_OS_WIN32
	/* Stupid. ':' and '\\' are encoded and base href with them
	 * doesn't "work". */
	if (local)
		encode_win32_uri_string(page, dirpath.source, dirpath.length);
	else
#endif
		encode_uri_string(page, dirpath.source, dirpath.length, 0);

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
		unsigned char *slash = dirpath.source;
		unsigned char *pslash = slash;
		unsigned char sep = local ? CHAR_DIR_SEP :  '/';

		while ((slash = strchr(slash, sep))) {
			/* FIXME: htmlesc? At least we should escape quotes. --pasky */
			if (!add_to_string(page, "<a href=\"")
	    		    || !add_string_to_string(page, &location)
			    || !add_bytes_to_string(page, dirpath.source, slash - dirpath.source)
			    || !add_char_to_string(page, sep)
			    || !add_to_string(page, "\">")
			    || !add_html_to_string(page, pslash, slash - pslash)
			    || !add_to_string(page, "</a>")
			    || !add_char_to_string(page, sep))
				goto out_of_memory;

			pslash = ++slash;
		}
	}

	if (!add_to_string(page, "</h2>\n<pre>")) {
out_of_memory:
		done_string(page);
	}

	done_string(&dirpath);
	done_string(&location);

	return page->length > 0 ? S_OK : S_OUT_OF_MEM;
}
