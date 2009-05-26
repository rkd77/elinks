/* Test library */

#ifndef EL__UTIL_TEST_H
#define EL__UTIL_TEST_H

#include <stdlib.h>

static inline void
#if (__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2
__attribute__((noreturn))
#endif
die(const char *msg, ...)
{
	va_list args;

	if (msg) {
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		fputs("\n", stderr);
		va_end(args);
	}

	exit(EXIT_FAILURE);
}

static inline int
get_test_opt(char **argref, const char *name, int *argi, int argc, char *argv[],
	     const char *expect_msg)
{
	char *arg = *argref;
	int namelen = strlen(name);

	if (strncmp(arg, name, namelen))
		return 0;

	arg += namelen;
	if (*arg == '=') {
		(*argref) = arg + 1;

	} else if (!*arg) {
		(*argi)++;
		if ((*argi) >= argc)
			die("--%s expects %s", name, expect_msg);
		(*argref) = argv[(*argi)];

	} else {
		return 0;
	}

	return 1;
}

#endif
