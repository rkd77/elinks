/* Copyright (C) 1995-1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.

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

#include <string.h>

#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>

#include "elinks.h"

#include "intl/gettext/loadinfo.h"
#include "util/conv.h"
#include "util/string.h"

/* Awful hack to permit compilation under cygwin and its broken configure.
 * Configure script detects these functions, but compilation clashes on
 * implicit declarations of them... So we force use of internal ones.
 * Cygwin argz.h do not contain any declaration for these, nor any other
 * header while they are available in some linked libs.
 * Feel free to provide a better fix if any. --Zas */
#ifdef HAVE_SYS_CYGWIN_H
#undef HAVE___ARGZ_STRINGIFY
#undef HAVE___ARGZ_COUNT
#undef HAVE___ARGZ_NEXT
#else
#if defined HAVE_ARGZ_H
#include <argz.h>
#endif
#endif

/* On some strange systems still no definition of NULL is found.  Sigh!  */
#ifndef NULL
#if defined __STDC__ && __STDC__
#define NULL ((void *) 0)
#else
#define NULL 0
#endif
#endif

/* Define function which are usually not available.  */

#if !defined HAVE___ARGZ_COUNT
/* Returns the number of strings in ARGZ.  */
static size_t argz_count__(const unsigned char *argz, size_t len);

static size_t
argz_count__(const unsigned char *argz, size_t len)
{
	size_t count = 0;

	while (len > 0) {
		size_t part_len = strlen(argz);

		argz += part_len + 1;
		len -= part_len + 1;
		count++;
	}
	return count;
}

#undef __argz_count
#define __argz_count(argz, len) argz_count__ (argz, len)
#endif /* !HAVE___ARGZ_COUNT */

#if !defined HAVE___ARGZ_STRINGIFY
/* Make '\0' separated arg vector ARGZ printable by converting all the '\0's
   except the last into the character SEP.  */
static void argz_stringify__(unsigned char *argz, size_t len, int sep);

static void
argz_stringify__(unsigned char *argz, size_t len, int sep)
{
	while (len > 0) {
		size_t part_len = strlen(argz);

		argz += part_len;
		len -= part_len + 1;
		if (len > 0)
			*argz++ = sep;
	}
}

#undef __argz_stringify
#define __argz_stringify(argz, len, sep) argz_stringify__ (argz, len, sep)
#endif /* !HAVE___ARGZ_STRINGIFY */

#if !defined HAVE___ARGZ_NEXT
static unsigned char *argz_next__(unsigned char *argz, size_t argz_len,
			 const unsigned char *entry);

static unsigned char *
argz_next__(unsigned char *argz, size_t argz_len, const unsigned char *entry)
{
	if (entry) {
		if (entry < argz + argz_len)
			entry = strchr(entry, '\0') + 1;

		return entry >= argz + argz_len ? NULL : (unsigned char *) entry;
	} else if (argz_len > 0)
		return argz;
	else
		return 0;
}

#undef __argz_next
#define __argz_next(argz, len, entry) argz_next__ (argz, len, entry)
#endif /* !HAVE___ARGZ_NEXT */

/* Return number of bits set in X.  */
static inline int
pop(int x)
{
	/* We assume that no more than 16 bits are used.  */
	x = ((x & ~0x5555) >> 1) + (x & 0x5555);
	x = ((x & ~0x3333) >> 2) + (x & 0x3333);
	x = ((x >> 4) + x) & 0x0f0f;
	x = ((x >> 8) + x) & 0xff;

	return x;
}

struct loaded_l10nfile *
_nl_make_l10nflist(struct loaded_l10nfile **l10nfile_list,
		   const unsigned char *dirlist,
		   size_t dirlist_len,
		   int mask,
		   const unsigned char *language,
		   const unsigned char *territory,
		   const unsigned char *codeset,
		   const unsigned char *normalized_codeset,
		   const unsigned char *modifier,
		   const unsigned char *special,
		   const unsigned char *sponsor,
		   const unsigned char *revision,
		   const unsigned char *filename,
		   int do_allocate)
{
	unsigned char *abs_filename, *abs_langdirname;
	int abs_langdirnamelen;
	struct loaded_l10nfile *last = NULL;
	struct loaded_l10nfile *retval;
	unsigned char *cp;
	size_t entries;
	int cnt;

	/* Allocate room for the full file name.  */
	abs_filename = (unsigned char *) malloc(dirlist_len + strlen(language)
				       + ((mask & TERRITORY) != 0
					  ? strlen(territory) + 1 : 0)
				       + ((mask & XPG_CODESET) != 0
					  ? strlen(codeset) + 1 : 0)
				       + ((mask & XPG_NORM_CODESET) != 0
					  ? strlen(normalized_codeset) + 1 : 0)
				       + (((mask & XPG_MODIFIER) != 0
					   || (mask & CEN_AUDIENCE) != 0)
					  ? strlen(modifier) + 1 : 0)
				       + ((mask & CEN_SPECIAL) != 0
					  ? strlen(special) + 1 : 0)
				       + (((mask & CEN_SPONSOR) != 0
					   || (mask & CEN_REVISION) != 0)
					  ? (1 + ((mask & CEN_SPONSOR) != 0
						  ? strlen(sponsor) + 1 : 0)
					     + ((mask & CEN_REVISION) != 0
						? strlen(revision) +
						1 : 0)) : 0)
				       + 1 + strlen(filename) + 1);

	if (abs_filename == NULL)
		return NULL;

	retval = NULL;
	last = NULL;

	/* Construct file name.  */
	memcpy(abs_filename, dirlist, dirlist_len);
	__argz_stringify(abs_filename, dirlist_len, PATH_SEPARATOR);
	cp = abs_filename + (dirlist_len - 1);
	*cp++ = '/';
	abs_langdirname = cp;
	cp = stpcpy(cp, language);

	if ((mask & TERRITORY) != 0) {
		*cp++ = '_';
		cp = stpcpy(cp, territory);
	}
	if ((mask & XPG_CODESET) != 0) {
		*cp++ = '.';
		cp = stpcpy(cp, codeset);
	}
	if ((mask & XPG_NORM_CODESET) != 0) {
		*cp++ = '.';
		cp = stpcpy(cp, normalized_codeset);
	}
	if ((mask & (XPG_MODIFIER | CEN_AUDIENCE)) != 0) {
		/* This component can be part of both syntaces but has different
		   leading characters.  For CEN we use `+', else `@'.  */
		*cp++ = (mask & CEN_AUDIENCE) != 0 ? '+' : '@';
		cp = stpcpy(cp, modifier);
	}
	if ((mask & CEN_SPECIAL) != 0) {
		*cp++ = '+';
		cp = stpcpy(cp, special);
	}
	if ((mask & (CEN_SPONSOR | CEN_REVISION)) != 0) {
		*cp++ = ',';
		if ((mask & CEN_SPONSOR) != 0)
			cp = stpcpy(cp, sponsor);
		if ((mask & CEN_REVISION) != 0) {
			*cp++ = '_';
			cp = stpcpy(cp, revision);
		}
	}
	abs_langdirnamelen = cp - abs_langdirname;

	*cp++ = '/';
	stpcpy(cp, filename);

	/* Look in list of already loaded domains whether it is already
	   available.  */
	last = NULL;
	for (retval = *l10nfile_list; retval != NULL; retval = retval->next)
		if (retval->filename != NULL) {
			int compare = strcmp(retval->filename, abs_filename);

			if (compare == 0)
				/* We found it!  */
				break;
			if (compare < 0) {
				/* It's not in the list.  */
				retval = NULL;
				break;
			}

			last = retval;
		}

	if (retval != NULL || do_allocate == 0) {
		free(abs_filename);
		return retval;
	}

	retval = (struct loaded_l10nfile *)
		malloc(sizeof(*retval) + (__argz_count(dirlist, dirlist_len)
					  * (1 << pop(mask))
					  * sizeof(struct loaded_l10nfile *)));
	if (retval == NULL)
		return NULL;

	retval->filename = abs_filename;
	retval->langdirname = abs_langdirname;
	retval->langdirnamelen = abs_langdirnamelen;
	retval->decided = (__argz_count(dirlist, dirlist_len) != 1
			   || ((mask & XPG_CODESET) != 0
			       && (mask & XPG_NORM_CODESET) != 0));
	retval->data = NULL;

	if (last == NULL) {
		retval->next = *l10nfile_list;
		*l10nfile_list = retval;
	} else {
		retval->next = last->next;
		last->next = retval;
	}

	entries = 0;
	/* If the DIRLIST is a real list the RETVAL entry corresponds not to
	   a real file.  So we have to use the DIRLIST separation mechanism
	   of the inner loop.  */
	cnt = __argz_count(dirlist, dirlist_len) == 1 ? mask - 1 : mask;
	for (; cnt >= 0; --cnt)
		if ((cnt & ~mask) == 0
		    && ((cnt & CEN_SPECIFIC) == 0 || (cnt & XPG_SPECIFIC) == 0)
		    && ((cnt & XPG_CODESET) == 0
			|| (cnt & XPG_NORM_CODESET) == 0)) {
			/* Iterate over all elements of the DIRLIST.  */
			unsigned char *dir = NULL;

			while ((dir =
				__argz_next((unsigned char *) dirlist, dirlist_len, dir))
			       != NULL)
				retval->successor[entries++]
					= _nl_make_l10nflist(l10nfile_list, dir,
							     strlen(dir) + 1,
							     cnt, language,
							     territory, codeset,
							     normalized_codeset,
							     modifier, special,
							     sponsor, revision,
							     filename, 1);
		}
	retval->successor[entries] = NULL;

	return retval;
}

/* Normalize codeset name.  There is no standard for the codeset
   names.  Normalization allows the user to use any of the common
   names.  The return value is dynamically allocated and has to be
   freed by the caller.  */
const unsigned char *
_nl_normalize_codeset(const unsigned char *codeset, size_t name_len)
{
	int len = 0;
	int only_digit = 1;
	unsigned char *retval;
	unsigned char *wp;
	size_t cnt;

	for (cnt = 0; cnt < name_len; ++cnt)
		if (isalnum(codeset[cnt])) {
			++len;

			if (isalpha(codeset[cnt]))
				only_digit = 0;
		}

	retval = (unsigned char *) malloc((only_digit ? 3 : 0) + len + 1);

	if (retval != NULL) {
		if (only_digit)
			wp = stpcpy(retval, "iso");
		else
			wp = retval;

		for (cnt = 0; cnt < name_len; ++cnt)
			if (isalpha(codeset[cnt]))
				*wp++ = c_tolower(codeset[cnt]);
			else if (isdigit(codeset[cnt]))
				*wp++ = codeset[cnt];

		*wp = '\0';
	}

	return (const unsigned char *) retval;
}
