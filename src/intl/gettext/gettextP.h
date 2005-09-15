/* Header describing internals of libintl library.
   Copyright (C) 1995-1999, 2000, 2001 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@cygnus.com>, 1995.

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

#ifndef _GETTEXTP_H
#define _GETTEXTP_H

#include <stddef.h>		/* Get size_t.  */

#include "util/string.h"	/* Get possible stubs.  */

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include "intl/gettext/loadinfo.h"
#include "intl/gettext/gettext.h"		/* Get nls_uint32.  */

#ifndef W
#define W(flag, data) ((flag) ? SWAP (data) : (data))
#endif

static inline nls_uint32
SWAP(nls_uint32 i)
{
	return (i << 24) | ((i & 0xff00) << 8) | ((i >> 8) & 0xff00) | (i >>
									24);
}

/* This is the representation of the expressions to determine the
   plural form.  */
struct expression {
	int nargs;		/* Number of arguments.  */
	enum operator {
		/* Without arguments:  */
		var,		/* The variable "n".  */
		num,		/* Decimal number.  */
		/* Unary operators:  */
		lnot,		/* Logical NOT.  */
		/* Binary operators:  */
		mult,		/* Multiplication.  */
		divide,		/* Division.  */
		module,		/* Module operation.  */
		plus,		/* Addition.  */
		minus,		/* Subtraction.  */
		less_than,	/* Comparison.  */
		greater_than,	/* Comparison.  */
		less_or_equal,	/* Comparison.  */
		greater_or_equal,	/* Comparison.  */
		equal,		/* Comparision for equality.  */
		not_equal,	/* Comparision for inequality.  */
		land,		/* Logical AND.  */
		lor,		/* Logical OR.  */
		/* Ternary operators:  */
		qmop		/* Question mark operator.  */
	} operation;
	union {
		unsigned long int num;	/* Number value for `num'.  */
		struct expression *args[3];	/* Up to three arguments.  */
	} val;
};

/* This is the data structure to pass information to the parser and get
   the result in a thread-safe way.  */
struct parse_args {
	const unsigned char *cp;
	struct expression *res;
};

/* The representation of an opened message catalog.  */
struct loaded_domain {
	const unsigned char *data;
	int use_mmap;
	size_t mmap_size;
	int must_swap;
	nls_uint32 nstrings;
	struct string_desc *orig_tab;
	struct string_desc *trans_tab;
	nls_uint32 hash_size;
	nls_uint32 *hash_tab;
	int codeset_cntr;
#if HAVE_ICONV
	iconv_t conv;
#endif
	unsigned char **conv_tab;

	struct expression *plural;
	unsigned long int nplurals;
};

/* A set of settings bound to a message domain.  Used to store settings
   from bindtextdomain() and bind_textdomain_codeset().  */
struct binding {
	struct binding *next;
	unsigned char *dirname;
	int codeset_cntr;	/* Incremented each time codeset changes.  */
	unsigned char *codeset;
	unsigned char domainname[1];
};

extern unsigned char *LANGUAGE;

/* A counter which is incremented each time some previous translations
   become invalid.
   This variable is part of the external ABI of the GNU libintl.  */
extern int _nl_msg_cat_cntr;

struct loaded_l10nfile *_nl_find_domain(const unsigned char *__dirname,
					unsigned char *__locale,
					const unsigned char *__domainname,
					struct binding *
					__domainbinding);
void _nl_load_domain(struct loaded_l10nfile * __domain,
		     struct binding * __domainbinding);
void _nl_unload_domain(struct loaded_domain * __domain);
const unsigned char *_nl_init_domain_conv(struct loaded_l10nfile * __domain_file,
				 struct loaded_domain * __domain,
				 struct binding * __domainbinding);
void _nl_free_domain_conv(struct loaded_domain * __domain);

unsigned char *_nl_find_msg(struct loaded_l10nfile * domain_file,
		   struct binding * domainbinding,
		   const unsigned char *msgid, size_t * lengthp);

extern unsigned char *gettext__(const unsigned char *__msgid);
extern unsigned char *dgettext__(const unsigned char *__domainname, const unsigned char *__msgid);
extern unsigned char *dcgettext__(const unsigned char *__domainname,
			 const unsigned char *__msgid, int __category);
extern unsigned char *ngettext__(const unsigned char *__msgid1, const unsigned char *__msgid2,
			unsigned long int __n);
extern unsigned char *dngettext__(const unsigned char *__domainname,
			 const unsigned char *__msgid1, const unsigned char *__msgid2,
			 unsigned long int __n);
extern unsigned char *dcngettext__(const unsigned char *__domainname,
			  const unsigned char *__msgid1, const unsigned char *__msgid2,
			  unsigned long int __n, int __category);
extern unsigned char *dcigettext__(const unsigned char *__domainname,
			  const unsigned char *__msgid1, const unsigned char *__msgid2,
			  int __plural, unsigned long int __n,
			  int __category);
extern unsigned char *textdomain__(const unsigned char *__domainname);
extern unsigned char *bindtextdomain__(const unsigned char *__domainname,
			      const unsigned char *__dirname);
extern unsigned char *bind_textdomain_codeset__(const unsigned char *__domainname,
				       const unsigned char *__codeset);

extern void gettext_free_exp__(struct expression * exp);
extern int gettext__parse(void *arg);

#endif /* gettextP.h  */
