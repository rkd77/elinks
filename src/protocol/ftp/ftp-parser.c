/* Tool for testing the FTP parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "osdep/stat.h"
#include "protocol/ftp/parse.h"
#include "util/test.h"

int
main(int argc, char *argv[])
{
	struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO;
	unsigned char *response = "";
	int i;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (strncmp(arg, "--", 2))
			break;

		arg += 2;

		if (get_test_opt(&arg, "response", &i, argc, argv, "a string")) {
			response = arg;

		} else {
			die("Unknown argument '%s'", arg - 2);
		}
	}

	if (!*response)
		die("Usage: %s --response \"string\"", argv[0]);

	while (*response) {
		unsigned char *start = response;

		response = strchr(response, '\n');
		if (!response) {
			response = start + strlen(start);
		} else {
			if (response > start && response[-1] == '\r')
				response[-1] = 0;
			*response++ = 0;
		}

		if (!parse_ftp_file_info(&ftp_info, start, strlen(start)))
			return 1;
	}

	return 0;
}
