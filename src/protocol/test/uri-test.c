#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

int
get_https_by_default(void)
{
	return 0;
}

const char *
get_default_protocol(void)
{
	return "file://";
}

#include "protocol/uri.h"
#include "util/string.h"

/* fake tty get function, needed for charsets.c */
int get_ctl_handle()
{
	return -1;
}

int
main(int argc, char **argv)
{
	/* FIXME: As more protocol tests are added this could start
	 * taking arguments like --normalize-uri=<arg> etc. */
	if (argc == 2) {
		fprintf(stdout, "%s\n", normalize_uri(NULL, argv[1]));
	} else if (argc == 3) {
		struct uri *translated = get_translated_uri(argv[1], argv[2]);
		if (translated == NULL)
			return EXIT_FAILURE;
		fprintf(stdout, "%s\n", struri(translated));
	}
	return 0;
}
