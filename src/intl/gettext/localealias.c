/* Handle aliases for locale names.
   Copyright (C) 1995-1999, 2000, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Tell glibc's <string.h> to provide a prototype for mempcpy().
   This must come before <config.h> because <config.h> may include
   <features.h>, and once <features.h> has been included, it's too late.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE    1
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/gettextP.h"
#include "util/memory.h"
#include "util/string.h"

#ifdef HAVE_FGETS_UNLOCKED
#undef fgets
#define fgets(buf, len, s) fgets_unlocked (buf, len, s)
#endif
#ifdef HAVE_FEOF_UNLOCKED
#undef feof
#define feof(s) feof_unlocked (s)
#endif

struct alias_map {
	const unsigned char *alias;
	const unsigned char *value;
};

static unsigned char *string_space;
static size_t string_space_act;
static size_t string_space_max;
static struct alias_map *map;
static size_t nmap;
static size_t maxmap;

/* Prototypes for local functions.  */
static size_t read_alias_file(const unsigned char *fname, int fname_len);
static int extend_alias_table(void);
static int alias_compare(const struct alias_map * map1,
			 const struct alias_map * map2);

const unsigned char *
_nl_expand_alias(const unsigned char *name)
{
	static const unsigned char *locale_alias_path = LOCALEDIR;
	struct alias_map *retval;
	const unsigned char *result = NULL;
	size_t added;

	do {
		struct alias_map item;

		item.alias = name;

		if (nmap > 0)
			retval = (struct alias_map *) bsearch(&item, map, nmap,
							      sizeof(struct
								     alias_map),
							      (int (*)
							       (const void *,
								const void *)
							      ) alias_compare);
		else
			retval = NULL;

		/* We really found an alias.  Return the value.  */
		if (retval != NULL) {
			result = retval->value;
			break;
		}

		/* Perhaps we can find another alias file.  */
		added = 0;
		while (added == 0 && locale_alias_path[0] != '\0') {
			const unsigned char *start;

			while (locale_alias_path[0] == PATH_SEPARATOR)
				++locale_alias_path;
			start = locale_alias_path;

			while (locale_alias_path[0] != '\0'
			       && locale_alias_path[0] != PATH_SEPARATOR)
				++locale_alias_path;

			if (start < locale_alias_path)
				added = read_alias_file(start,
							locale_alias_path -
							start);
		}
	}
	while (added != 0);

	return result;
}

static size_t
read_alias_file(const unsigned char *fname, int fname_len)
{
	FILE *fp;
	unsigned char *full_fname;
	size_t added;
	static const unsigned char aliasfile[] = "/locale.alias";

	full_fname = (unsigned char *) fmem_alloc(fname_len + sizeof(aliasfile));
	mempcpy(mempcpy(full_fname, fname, fname_len),
		aliasfile, sizeof(aliasfile));

	fp = fopen(full_fname, "rb");
	fmem_free(full_fname);
	if (fp == NULL)
		return 0;

	added = 0;
	while (!feof(fp)) {
		/* It is a reasonable approach to use a fix buffer here because
		   a) we are only interested in the first two fields
		   b) these fields must be usable as file names and so must not
		   be that long
		 */
		unsigned char buf[BUFSIZ];
		unsigned char *alias;
		unsigned char *value;
		unsigned char *cp;

		if (fgets(buf, sizeof(buf), fp) == NULL)
			/* EOF reached.  */
			break;

		/* Possibly not the whole line fits into the buffer.  Ignore
		   the rest of the line.  */
		if (strchr(buf, '\n') == NULL) {
			unsigned char altbuf[BUFSIZ];

			do
				if (fgets(altbuf, sizeof(altbuf), fp) == NULL)
					/* Make sure the inner loop will be left.  The outer loop
					   will exit at the `feof' test.  */
					break;
			while (strchr(altbuf, '\n') == NULL);
		}

		cp = buf;
		/* Ignore leading white space.  */
		skip_space(cp);

		/* A leading '#' signals a comment line.  */
		if (cp[0] != '\0' && cp[0] != '#') {
			alias = cp++;
			skip_nonspace(cp);
			/* Terminate alias name.  */
			if (cp[0] != '\0')
				*cp++ = '\0';

			/* Now look for the beginning of the value.  */
			skip_space(cp);

			if (cp[0] != '\0') {
				size_t alias_len;
				size_t value_len;

				value = cp++;
				skip_nonspace(cp);
				/* Terminate value.  */
				if (cp[0] == '\n') {
					/* This has to be done to make the following test
					   for the end of line possible.  We are looking for
					   the terminating '\n' which do not overwrite here.  */
					*cp++ = '\0';
					*cp = '\n';
				} else if (cp[0] != '\0')
					*cp++ = '\0';

				if (nmap >= maxmap)
					if (extend_alias_table())
						return added;

				alias_len = strlen(alias) + 1;
				value_len = strlen(value) + 1;

				if (string_space_act + alias_len + value_len >
				    string_space_max) {
					/* Increase size of memory pool.  */
					size_t new_size = (string_space_max
							   + (alias_len +
							      value_len >
							      1024 ? alias_len +
							      value_len :
							      1024));
					unsigned char *new_pool =
						(unsigned char *) realloc(string_space,
								 new_size);
					if (new_pool == NULL)
						return added;

					if (string_space != new_pool) {
						size_t i;

						for (i = 0; i < nmap; i++) {
							map[i].alias +=
								new_pool -
								string_space;
							map[i].value +=
								new_pool -
								string_space;
						}
					}

					string_space = new_pool;
					string_space_max = new_size;
				}

				map[nmap].alias =
					memcpy(&string_space[string_space_act],
					       alias, alias_len);
				string_space_act += alias_len;

				map[nmap].value =
					memcpy(&string_space[string_space_act],
					       value, value_len);
				string_space_act += value_len;

				++nmap;
				++added;
			}
		}
	}

	/* Should we test for ferror()?  I think we have to silently ignore
	   errors.  --drepper  */
	fclose(fp);

	if (added > 0)
		qsort(map, nmap, sizeof(struct alias_map),
		      (int (*)(const void *, const void *))
		      alias_compare);

	return added;
}

static int
extend_alias_table(void)
{
	size_t new_size;
	struct alias_map *new_map;

	new_size = maxmap == 0 ? 100 : 2 * maxmap;
	new_map = (struct alias_map *) realloc(map, (new_size
						     *
						     sizeof(struct alias_map)));
	if (new_map == NULL)
		/* Simply don't extend: we don't have any more core.  */
		return -1;

	map = new_map;
	maxmap = new_size;
	return 0;
}

static int
alias_compare(const struct alias_map *map1, const struct alias_map *map2)
{
	return c_strcasecmp(map1->alias, map2->alias);
}
