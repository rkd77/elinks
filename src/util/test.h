/* Test library */

#ifndef EL__UTIL_TEST_H
#define EL__UTIL_TEST_H

#include <stdlib.h>

static inline void
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

#endif
