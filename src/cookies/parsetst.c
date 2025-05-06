/* Tool for testing of cookies string parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cookies/parser.h"

/* fake tty get function, needed for charsets.c */
int get_ctl_handle()
{
	ELOG
	return -1;
}

char *
gettext(const char *text)
{
	ELOG
	return (char *)text;
}

int
os_default_charset(void)
{
	ELOG
	return -1;
}

int
main(int argc, char *argv[])
{
	ELOG
	struct cookie_str cstr;
	char name[1024], value[1024], string[1024];

	printf("This thing is for testing of cookies name-value pair parser.\n"
	       "You probably do not want to mess with it :).\n");

	while (1) {
		printf("Enter string (empty==quit): "); fflush(stdout);

		fgets(string, 1024, stdin);

		string[strlen(string) - 1] = '\0'; /* Strip newline. */
		if (!*string) return 0;

		if (!parse_cookie_str(&cstr, string)) {
			printf("ERROR while parsing '%s'!\n", string);
			continue;
		}

		memcpy(name, cstr.str, cstr.nam_end - cstr.str);
		name[cstr.nam_end - cstr.str] = '\0';
		memcpy(value, cstr.val_start, cstr.val_end - cstr.val_start);
		value[cstr.val_end - cstr.val_start] = '\0';

		printf("'%s' -> '%s' :: '%s'\n", string, name, value);
	}
}
