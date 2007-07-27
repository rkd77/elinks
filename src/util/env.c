/** Environment variables handling
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "util/env.h"
#include "util/memory.h"
#include "util/string.h"

/** Set @a name environment variable to @a value or a substring of it:
 * On success, it returns 0.
 * If @a value is NULL and on error, it returns -1.
 * If @a length >= 0 and smaller than true @a value length, it will
 * set @a name to specified substring of @a value.
 */
int
env_set(unsigned char *name, unsigned char *value, int length)
{
	int true_length, substring = 0;

	if (!value || !name || !*name) return -1;

	true_length = strlen(value);
	substring = (length >= 0 && length < true_length);
	if (!substring) length = true_length;

#if defined(HAVE_SETENV)
	{
		int ret, allocated = 0;

		if (substring) {
			/* Copy the substring. */
			value = memacpy(value, length);
			if (!value) return -1;
			allocated = 1;
		}

		ret = setenv(name, value, 1);
		if (allocated) mem_free(value);
		return ret;
	}

#elif defined(HAVE_PUTENV)

	{
		int namelen = strlen(name);
		char *s = malloc(namelen + length + 2);

		if (!s) return -1;
		memcpy(s, name, namelen);
		s[namelen] = '=';
		memcpy(&s[namelen + 1], value, length);
		s[namelen + 1 + length] = '\0';

		/* @s is never freed by us, this is intentional.
		 * --> Possible memleaks on repeated use of
		 * this function. */
		return putenv(s);
	}

#else
	/* XXX: what to do ?? */
	return -1;
#endif
}
