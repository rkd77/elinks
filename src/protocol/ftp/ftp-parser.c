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
	int responselen = 0;
	int i;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (strncmp(arg, "--", 2))
			break;

		arg += 2;

		if (get_test_opt(&arg, "response", &i, argc, argv, "a string")) {
			response = arg;
			responselen = strlen(response);

		} else {
			die("Unknown argument '%s'", arg - 2);
		}
	}

	if (!responselen)
		die("Usage: %s --response \"string\"", argv[0]);

	if (parse_ftp_file_info(&ftp_info, response, responselen))
		return 0;

	return 1;
}
