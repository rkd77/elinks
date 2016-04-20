/* Parse <meta http-equiv="refresh" content="..."> */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>

#include "elinks.h"

#include "document/html/parse-meta-refresh.h"
#include "osdep/ascii.h"
#include "util/string.h"

#define LWS(c) ((c) == ' ' || (c) == ASCII_TAB)

int
html_parse_meta_refresh(const unsigned char *content,
			unsigned long *delay_out,
			unsigned char **url_out)
{
	const unsigned char *scan = content;
	char *delay_end;
	int saw_delay = 0;
	const unsigned char *url_begin;
	const unsigned char *url_end;

	*url_out = NULL;
	*delay_out = 0;

	while (LWS(*scan))
		++scan;

	/* TODO: Do we need to switch to the "C" locale and back?  */
	*delay_out = strtoul(scan, &delay_end, 10);
	saw_delay = (scan != (const unsigned char *) delay_end);
	scan = (const unsigned char *) delay_end;

	if (saw_delay) {
		/* Omit any fractional part.  */
		if (*scan == '.') {
			++scan;
			while (!(*scan == '\0' || LWS(*scan)
				 || *scan == ';' || *scan == ','))
				++scan;
		}

		if (!(*scan == '\0' || LWS(*scan)
		      || *scan == ';' || *scan == ',')) {
			/* The delay is followed by garbage.  Give up.  */
			return -1;
		}

		/* Between the delay and the URL, there must be at
		 * least one LWS, semicolon, or comma; optionally with
		 * more LWS around it.  */
		while (LWS(*scan))
			++scan;
		if (*scan == ';' || *scan == ',')
			++scan;
	} else {
		/* The delay was not specified.  The delimiter must be
		 * a semicolon or a comma, optionally with LWS.  LWS
		 * alone does not suffice.  */
		while (*scan != '\0' && *scan != ';' && *scan != ',')
			++scan;
		if (*scan == ';' || *scan == ',')
			++scan;
	}
	
	while (LWS(*scan))
		++scan;

	/* Presume the URL begins here...  */
	url_begin = scan;

	/* ..unless there is "URL=" with at least one equals sign,
	 * and optional spaces.  */
	if ((scan[0] == 'U' || scan[0] == 'u')
	    && (scan[1] == 'R' || scan[1] == 'r')
	    && (scan[2] == 'L' || scan[2] == 'l')) {
		scan += 3;
 		while (LWS(*scan))
			++scan;
		if (*scan == '=') {
			++scan;
			while (LWS(*scan))
				++scan;
			url_begin = scan;
		}
	}

	if (*url_begin == '"' || *url_begin == '\'') {
		unsigned char quote = *url_begin++;

		url_end = strchr((const char *)url_begin, quote);
		if (url_end == NULL)
			url_end = strchr((const char *)url_begin, '\0');
	} else {
		url_end = strchr((const char *)url_begin, '\0');
	}

	/* In any case, trim all spaces from the end of the URL.  */
	while (url_begin < url_end && LWS(url_end[-1]))
		--url_end;

	if (url_begin != url_end) {
		*url_out = memacpy(url_begin, url_end - url_begin);
		if (!*url_out)
			return -1;
	} else if (!saw_delay) {
		/* There is no delay and no URL.  */
		return -1;
	}
	
	return 0;
}
