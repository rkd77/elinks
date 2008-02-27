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


void die(const char *msg, ...)
{
	va_list args;

	if (msg) {
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		fputs("\n", stderr);
		va_end(args);
	}

	exit(!!NULL);
}

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

		if (!strncmp(arg, "response", 8)) {
			arg += 8;
			if (*arg == '=') {
				arg++;
				response = arg;
			} else {
				i++;
				if (i >= argc)
					die("--response expects a string");
				response = argv[i];
			}

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
