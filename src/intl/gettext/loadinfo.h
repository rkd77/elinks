/* Copyright (C) 1996-1999, 2000, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1996.

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

#ifndef _LOADINFO_H
#define _LOADINFO_H	1

/* Separator in PATH like lists of pathnames.  */
#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
  /* Win32, OS/2, DOS */
#define PATH_SEPARATOR ';'
#else
  /* Unix */
#define PATH_SEPARATOR ':'
#endif

/* Encoding of locale name parts.  */
#define CEN_REVISION		1
#define CEN_SPONSOR		2
#define CEN_SPECIAL		4
#define XPG_NORM_CODESET	8
#define XPG_CODESET		16
#define TERRITORY		32
#define CEN_AUDIENCE		64
#define XPG_MODIFIER		128

#define CEN_SPECIFIC	(CEN_REVISION|CEN_SPONSOR|CEN_SPECIAL|CEN_AUDIENCE)
#define XPG_SPECIFIC	(XPG_CODESET|XPG_NORM_CODESET|XPG_MODIFIER)

struct loaded_l10nfile {
	const unsigned char *filename;
	const unsigned char *langdirname;
	int langdirnamelen;

	int decided;

	const void *data;

	struct loaded_l10nfile *next;
	struct loaded_l10nfile *successor[1];
};

/* Normalize codeset name.  There is no standard for the codeset
   names.  Normalization allows the user to use any of the common
   names.  The return value is dynamically allocated and has to be
   freed by the caller.  */
extern const unsigned char *_nl_normalize_codeset(const unsigned char *codeset,
					 size_t name_len);

extern struct loaded_l10nfile *_nl_make_l10nflist(
        struct loaded_l10nfile ** l10nfile_list, const unsigned char *dirlist,
	size_t dirlist_len, int mask, const unsigned char *language,
	const unsigned char *territory, const unsigned char *codeset,
	const unsigned char *normalized_codeset, const unsigned char *modifier,
	const unsigned char *special, const unsigned char *sponsor, const unsigned char *revision,
	const unsigned char *filename, int do_allocate);

extern const unsigned char *_nl_expand_alias(const unsigned char *name);

/* normalized_codeset is dynamically allocated and has to be freed by
   the caller.  */
extern int _nl_explode_name(unsigned char *name, const unsigned char **language,
			    const unsigned char **modifier,
			    const unsigned char **territory,
			    const unsigned char **codeset,
			    const unsigned char **normalized_codeset,
			    const unsigned char **special,
			    const unsigned char **sponsor,
			    const unsigned char **revision);

extern unsigned char *_nl_find_language(const unsigned char *name);

#endif /* loadinfo.h */
