#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "protocol/uri.h"
#include "util/string.h"

int
main(int argc, char **argv)
{
	/* FIXME: As more protocol tests are added this could start
	 * taking arguments like --normalize-uri=<arg> etc. */
	if (argc == 2) {
		fprintf(stdout, "%s\n", normalize_uri(NULL, argv[1]));
	}
	return 0;
}
