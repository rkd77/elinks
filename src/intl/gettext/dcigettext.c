/* Implementation of the internal dcigettext function.
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

#include <sys/types.h>

#if defined HAVE_ALLOCA_H
#include <alloca.h>
#else
#ifdef _AIX
#pragma alloca
#else
#ifndef HAVE_ALLOCA
void *alloca(size_t size);
#endif
#endif
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif

#include <stddef.h>
#include <stdlib.h>

#include <string.h>

#if defined HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h> /* Needed for alloca() on MingW/Win32 */
#endif

#include "elinks.h"

#include "intl/gettext/gettextP.h"
#include "intl/gettext/libintl.h"
#include "intl/gettext/hash-string.h"
#include "util/string.h"

/* Amount to increase buffer size by in each try.  */
#define PATH_INCR 32

/* The following is from pathmax.h.  */
/* Non-POSIX BSD systems might have gcc's limits.h, which doesn't define
   PATH_MAX but might cause redefinition warnings when sys/param.h is
   later included (as on MORE/BSD 4.3).  */
#if defined _POSIX_VERSION || defined HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif

#if !defined PATH_MAX && defined _PC_PATH_MAX
#define PATH_MAX (pathconf ("/", _PC_PATH_MAX) < 1 ? 1024 : pathconf ("/", _PC_PATH_MAX))
#endif

/* Don't include sys/param.h if it already has been.  */
#if defined HAVE_SYS_PARAM_H && !defined PATH_MAX && !defined MAXPATHLEN
#include <sys/param.h>
#endif

#if !defined PATH_MAX && defined MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#endif

#ifndef PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#endif

/* Pathname support.
   IS_ABSOLUTE_PATH(P)  tests whether P is an absolute path.  If it is not,
                        it may be concatenated to a directory pathname.
   IS_PATH_WITH_DIR(P)  tests whether P contains a directory specification.
 */
#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
  /* Win32, OS/2, DOS */
#define HAS_DEVICE(P) (isasciialpha((P)[0]) && (P)[1] == ':')
#define IS_ABSOLUTE_PATH(P) (dir_sep((P)[0]) || HAS_DEVICE (P))
#define IS_PATH_WITH_DIR(P) (strchr (P, '/') || strchr (P, '\\') || HAS_DEVICE (P))
#else
  /* Unix */
#define IS_ABSOLUTE_PATH(P) dir_sep((P)[0])
#define IS_PATH_WITH_DIR(P) strchr (P, '/')
#endif

/* XPG3 defines the result of `setlocale (category, NULL)' as:
   ``Directs `setlocale()' to query `category' and return the current
     setting of `local'.''
   However it does not specify the exact format.  Neither do SUSV2 and
   ISO C 99.  So we can use this feature only on selected systems (e.g.
   those using GNU C Library).  */
#if (defined __GNU_LIBRARY__ && __GNU_LIBRARY__ >= 2)
#define HAVE_LOCALE_NULL
#endif

/* This is the type used for the search tree where known translations
   are stored. */
struct known_translation_t {
	/* Domain in which to search.  */
	unsigned char *domainname;

	/* The category.  */
	int category;

	/* State of the catalog counter at the point the string was found.  */
	int counter;

	/* Catalog where the string was found.  */
	struct loaded_l10nfile *domain;

	/* And finally the translation.  */
	const unsigned char *translation;
	size_t translation_length;

	/* Pointer to the string in question.  */
	unsigned char msgid[1];
};

/* Root of the search tree with known translations.  We can use this
   only if the system provides the `tsearch' function family.  */
#if defined HAVE_TSEARCH
#include <search.h>

static void *root;

/* Function to compare two entries in the table of known translations.  */
static int transcmp(const void *p1, const void *p2);


static int
transcmp(const void *p1, const void *p2)
{
	const struct known_translation_t *s1;
	const struct known_translation_t *s2;
	int result;

	s1 = (const struct known_translation_t *) p1;
	s2 = (const struct known_translation_t *) p2;

	result = strcmp(s1->msgid, s2->msgid);
	if (result == 0) {
		result = strcmp(s1->domainname, s2->domainname);
		if (result == 0)
			/* We compare the category last (though this is the cheapest
			   operation) since it is hopefully always the same (namely
			   LC_MESSAGES).  */
			result = s1->category - s2->category;
	}

	return result;
}
#endif

/* Name of the default domain used for gettext(3) prior any call to
   textdomain(3).  The default value for this is "messages".  */
const unsigned char _nl_default_default_domain__[] = "messages";

/* Value used as the default domain for gettext(3).  */
const unsigned char *_nl_current_default_domain__ = _nl_default_default_domain__;

/* Contains the default location of the message catalogs.  */
const unsigned char _nl_default_dirname__[] = LOCALEDIR;

/* Contains application-specific LANGUAGE variation, taking precedence to the
 * $LANGUAGE environment variable.  */
unsigned char *LANGUAGE = NULL;

/* List with bindings of specific domains created by bindtextdomain()
   calls.  */
struct binding *_nl_domain_bindings__;

/* Prototypes for local functions.  */
static unsigned char *plural_lookup(struct loaded_l10nfile * domain,
			   unsigned long int n,
			   const unsigned char *translation,
			   size_t translation_len);
static unsigned long int plural_eval(struct expression * pexp,
				     unsigned long int n);
static const unsigned char *category_to_name(int category);
static const unsigned char *guess_category_value(int category,
					const unsigned char *categoryname);

/* For those loosing systems which don't have `alloca' we have to add
   some additional code emulating it.  */
#ifdef HAVE_ALLOCA
/* Nothing has to be done.  */
#define ADD_BLOCK(list, address)	/* nothing */
#define FREE_BLOCKS(list)	/* nothing */
#else
struct block_list {
	void *address;
	struct block_list *next;
};
#define ADD_BLOCK(list, addr)						      \
  do {									      \
    struct block_list *newp = (struct block_list *) malloc(sizeof(*newp));    \
    /* If we cannot get a free block we cannot add the new element to	      \
       the list.  */							      \
    if (newp != NULL) {							      \
      newp->address = (addr);						      \
      newp->next = (list);						      \
      (list) = newp;							      \
    }									      \
  } while (0)
#define FREE_BLOCKS(list)						      \
  do {									      \
    while (list != NULL) {						      \
      struct block_list *old = list;					      \
      list = list->next;			      			\
      free (old->address); 						\
      free (old);							      \
    }									      \
  } while (0)
#undef alloca
#define alloca(size) (malloc (size))
#endif /* have alloca */

typedef unsigned char transmem_block_t;

/* Checking whether the binaries runs SUID must be done and glibc provides
   easier methods therefore we make a difference here.  */
#ifndef HAVE_GETUID
#define getuid() 0
#endif

#ifndef HAVE_GETGID
#define getgid() 0
#endif

#ifndef HAVE_GETEUID
#define geteuid() getuid()
#endif

#ifndef HAVE_GETEGID
#define getegid() getgid()
#endif

static int enable_secure;

#define ENABLE_SECURE (enable_secure == 1)
#define DETERMINE_SECURE \
  if (enable_secure == 0)						      \
    {									      \
      if (getuid () != geteuid () || getgid () != getegid ())		      \
	enable_secure = 1;						      \
      else								      \
	enable_secure = -1;						      \
    }

/* Look up MSGID in the DOMAINNAME message catalog for the current
   CATEGORY locale and, if PLURAL is nonzero, search over string
   depending on the plural form determined by N.  */
unsigned char *
dcigettext__(const unsigned char *domainname, const unsigned char *msgid1, const unsigned char *msgid2,
	   int plural, unsigned long int n, int category)
{
#ifndef HAVE_ALLOCA
	struct block_list *block_list = NULL;
#endif
	struct loaded_l10nfile *domain;
	struct binding *binding;
	const unsigned char *categoryname;
	const unsigned char *categoryvalue;
	unsigned char *dirname, *xdomainname;
	unsigned char *single_locale;
	unsigned char *retval;
	size_t retlen;
	int saved_errno;

#if defined HAVE_TSEARCH
	struct known_translation_t *search;
	struct known_translation_t **foundp = NULL;
	size_t msgid_len;
#endif
	size_t domainname_len;

	/* If no real MSGID is given return NULL.  */
	if (msgid1 == NULL)
		return NULL;

	/* If DOMAINNAME is NULL, we are interested in the default domain.  If
	   CATEGORY is not LC_MESSAGES this might not make much sense but the
	   definition left this undefined.  */
	if (domainname == NULL)
		domainname = _nl_current_default_domain__;

#if defined HAVE_TSEARCH
	msgid_len = strlen(msgid1) + 1;

	/* Try to find the translation among those which we found at
	   some time.  */
	search = (struct known_translation_t *)
		alloca(offsetof(struct known_translation_t, msgid) + msgid_len);

	memcpy(search->msgid, msgid1, msgid_len);
	search->domainname = (unsigned char *) domainname;
	search->category = category;

	foundp = (struct known_translation_t **) tfind(search, &root, transcmp);
	if (foundp != NULL && (*foundp)->counter == _nl_msg_cat_cntr) {
		/* Now deal with plural.  */
		if (plural)
			retval = plural_lookup((*foundp)->domain, n,
					       (*foundp)->translation,
					       (*foundp)->translation_length);
		else
			retval = (unsigned char *) (*foundp)->translation;

		return retval;
	}
#endif

	/* Preserve the `errno' value.  */
	saved_errno = errno;

	/* See whether this is a SUID binary or not.  */
	DETERMINE_SECURE;

	/* First find matching binding.  */
	for (binding = _nl_domain_bindings__; binding != NULL;
	    binding = binding->next) {
		int compare = strcmp(domainname, binding->domainname);

		if (compare == 0)
			/* We found it!  */
			break;
		if (compare < 0) {
			/* It is not in the list.  */
			binding = NULL;
			break;
		}
	}

	if (binding == NULL)
		dirname = (unsigned char *) _nl_default_dirname__;
	else if (IS_ABSOLUTE_PATH(binding->dirname))
		dirname = binding->dirname;
	else {
		/* We have a relative path.  Make it absolute now.  */
		size_t dirname_len = strlen(binding->dirname) + 1;
		size_t path_max;
		unsigned char *ret;

		path_max = (unsigned int) PATH_MAX;
		path_max += 2;	/* The getcwd docs say to do this.  */

		for (;;) {
			dirname = (unsigned char *) alloca(path_max + dirname_len);
			ADD_BLOCK(block_list, dirname);

			errno = 0;
			ret = getcwd(dirname, path_max);
			if (ret != NULL || errno != ERANGE)
				break;

			path_max += path_max / 2;
			path_max += PATH_INCR;
		}

		if (ret == NULL) {
			/* We cannot get the current working directory.  Don't signal an
			   error but simply return the default string.  */
			FREE_BLOCKS(block_list);
			errno = saved_errno;
			return (plural == 0 ? (unsigned char *) msgid1
				/* Use the Germanic plural rule.  */
				: n == 1 ? (unsigned char *) msgid1 : (unsigned char *) msgid2);
		}

		stpcpy(stpcpy(strchr(dirname, '\0'), "/"), binding->dirname);
	}

	/* Now determine the symbolic name of CATEGORY and its value.  */
	categoryname = category_to_name(category);
	categoryvalue = guess_category_value(category, categoryname);

	domainname_len = strlen(domainname);
	xdomainname = (unsigned char *) alloca(strlen(categoryname)
				      + domainname_len + 5);
	ADD_BLOCK(block_list, xdomainname);

	stpcpy(mempcpy(stpcpy(stpcpy(xdomainname, categoryname), "/"),
		       domainname, domainname_len), ".mo");

	/* Creating working area.  */
	single_locale = (unsigned char *) alloca(strlen(categoryvalue) + 1);
	ADD_BLOCK(block_list, single_locale);

	/* Search for the given string.  This is a loop because we perhaps
	   got an ordered list of languages to consider for the translation.  */
	while (1) {
		/* Make CATEGORYVALUE point to the next element of the list.  */
		while (categoryvalue[0] != '\0' && categoryvalue[0] == ':')
			++categoryvalue;
		if (categoryvalue[0] == '\0') {
			/* The whole contents of CATEGORYVALUE has been searched but
			   no valid entry has been found.  We solve this situation
			   by implicitly appending a "C" entry, i.e. no translation
			   will take place.  */
			single_locale[0] = 'C';
			single_locale[1] = '\0';
		} else {
			unsigned char *cp = single_locale;

			while (categoryvalue[0] != '\0'
			       && categoryvalue[0] != ':')
				*cp++ = *categoryvalue++;
			*cp = '\0';

			/* When this is a SUID binary we must not allow accessing files
			   outside the dedicated directories.  */
			if (ENABLE_SECURE && IS_PATH_WITH_DIR(single_locale))
				/* Ingore this entry.  */
				continue;
		}

		/* If the current locale value is C (or POSIX) we don't load a
		   domain.  Return the MSGID.  */
		if (strcmp(single_locale, "C") == 0
		    || strcmp(single_locale, "POSIX") == 0) {
			FREE_BLOCKS(block_list);
			errno = saved_errno;
			return (plural == 0 ? (unsigned char *) msgid1
				/* Use the Germanic plural rule.  */
				: n == 1 ? (unsigned char *) msgid1 : (unsigned char *) msgid2);
		}

		/* Find structure describing the message catalog matching the
		   DOMAINNAME and CATEGORY.  */
		domain = _nl_find_domain(dirname, single_locale, xdomainname,
					 binding);

		if (domain != NULL) {
			retval = _nl_find_msg(domain, binding, msgid1, &retlen);

			if (retval == NULL) {
				int cnt;

				for (cnt = 0; domain->successor[cnt] != NULL;
				    ++cnt) {
					retval = _nl_find_msg(domain->
							      successor[cnt],
							      binding, msgid1,
							      &retlen);

					if (retval != NULL) {
						domain = domain->successor[cnt];
						break;
					}
				}
			}

			if (retval != NULL) {
				/* Found the translation of MSGID1 in domain DOMAIN:
				   starting at RETVAL, RETLEN bytes.  */
				FREE_BLOCKS(block_list);
				errno = saved_errno;
#if defined HAVE_TSEARCH
				if (foundp == NULL) {
					/* Create a new entry and add it to the search tree.  */
					struct known_translation_t *newp;

					newp = (struct known_translation_t *)
						malloc(offsetof
						       (struct
							known_translation_t,
							msgid)
						       + msgid_len +
						       domainname_len + 1);
					if (newp != NULL) {
						newp->domainname =
							mempcpy(newp->msgid,
								msgid1,
								msgid_len);
						memcpy(newp->domainname,
						       domainname,
						       domainname_len + 1);
						newp->category = category;
						newp->counter =
							_nl_msg_cat_cntr;
						newp->domain = domain;
						newp->translation = retval;
						newp->translation_length =
							retlen;

						/* Insert the entry in the search tree.  */
						foundp = (struct
							  known_translation_t
							  **)
							tsearch(newp, &root,
								transcmp);
						if (foundp == NULL
						    || *foundp != newp)
							/* The insert failed.  */
							free(newp);
					}
				} else {
					/* We can update the existing entry.  */
					(*foundp)->counter = _nl_msg_cat_cntr;
					(*foundp)->domain = domain;
					(*foundp)->translation = retval;
					(*foundp)->translation_length = retlen;
				}
#endif
				/* Now deal with plural.  */
				if (plural)
					retval = plural_lookup(domain, n,
							       retval, retlen);

				return retval;
			}
		}
	}
	/* NOTREACHED */
}

unsigned char *
_nl_find_msg(struct loaded_l10nfile *domain_file,
			       struct binding *domainbinding,
			       const unsigned char *msgid, size_t *lengthp)
{
	struct loaded_domain *domain;
	size_t act;
	unsigned char *result;
	size_t resultlen;

	if (domain_file->decided == 0)
		_nl_load_domain(domain_file, domainbinding);

	if (domain_file->data == NULL)
		return NULL;

	domain = (struct loaded_domain *) domain_file->data;

	/* Locate the MSGID and its translation.  */
	if (domain->hash_size > 2 && domain->hash_tab != NULL) {
		/* Use the hashing table.  */
		nls_uint32 len = strlen(msgid);
		nls_uint32 hash_val = hash_string(msgid);
		nls_uint32 idx = hash_val % domain->hash_size;
		nls_uint32 incr = 1 + (hash_val % (domain->hash_size - 2));

		while (1) {
			nls_uint32 nstr =
				W(domain->must_swap, domain->hash_tab[idx]);

			if (nstr == 0)
				/* Hash table entry is empty.  */
				return NULL;

			/* Compare msgid with the original string at index nstr-1.
			   We compare the lengths with >=, not ==, because plural entries
			   are represented by strings with an embedded NUL.  */
			if (W
			    (domain->must_swap,
			     domain->orig_tab[nstr - 1].length) >= len
			    &&
			    (strcmp
			     (msgid,
			      domain->data + W(domain->must_swap,
					       domain->orig_tab[nstr -
								1].offset))
			     == 0)) {
				act = nstr - 1;
				goto found;
			}

			if (idx >= domain->hash_size - incr)
				idx -= domain->hash_size - incr;
			else
				idx += incr;
		}
		/* NOTREACHED */
	} else {
		/* Try the default method:  binary search in the sorted array of
		   messages.  */
		size_t top, bottom;

		bottom = 0;
		top = domain->nstrings;
		while (bottom < top) {
			int cmp_val;

			act = (bottom + top) / 2;
			cmp_val = strcmp(msgid, (domain->data
						 + W(domain->must_swap,
						     domain->orig_tab[act].
						     offset)));
			if (cmp_val < 0)
				top = act;
			else if (cmp_val > 0)
				bottom = act + 1;
			else
				goto found;
		}
		/* No translation was found.  */
		return NULL;
	}

found:
	/* The translation was found at index ACT.  If we have to convert the
	   string to use a different character set, this is the time.  */
	result = ((unsigned char *) domain->data
		  + W(domain->must_swap, domain->trans_tab[act].offset));
	resultlen = W(domain->must_swap, domain->trans_tab[act].length) + 1;

#if HAVE_ICONV
	if (domain->codeset_cntr
	    != (domainbinding != NULL ? domainbinding->codeset_cntr : 0)) {
		/* The domain's codeset has changed through bind_textdomain_codeset()
		   since the message catalog was initialized or last accessed.  We
		   have to reinitialize the converter.  */
		_nl_free_domain_conv(domain);
		_nl_init_domain_conv(domain_file, domain, domainbinding);
	}

	if (domain->conv != (iconv_t) - 1) {
		/* We are supposed to do a conversion.  First allocate an
		   appropriate table with the same structure as the table
		   of translations in the file, where we can put the pointers
		   to the converted strings in.
		   There is a slight complication with plural entries.  They
		   are represented by consecutive NUL terminated strings.  We
		   handle this case by converting RESULTLEN bytes, including
		   NULs.  */

		if (domain->conv_tab == NULL
		    && ((domain->conv_tab = (unsigned char **) calloc(domain->nstrings,
							     sizeof(unsigned char *)))
			== NULL))
			/* Mark that we didn't succeed allocating a table.  */
			domain->conv_tab = (unsigned char **) -1;

		if (domain->conv_tab == (unsigned char **) -1)
			/* Nothing we can do, no more memory.  */
			goto converted;

		if (domain->conv_tab[act] == NULL) {
			/* We haven't used this string so far, so it is not
			   translated yet.  Do this now.  */
			/* We use a bit more efficient memory handling.
			   We allocate always larger blocks which get used over
			   time.  This is faster than many small allocations.   */
#define INITIAL_BLOCK_SIZE	4080
			static unsigned char *freemem;
			static size_t freemem_size;

			const unsigned char *inbuf;
			unsigned char *outbuf;
			int malloc_count;
			transmem_block_t *transmem_list = NULL;

			inbuf = (const unsigned char *) result;
			outbuf = freemem + sizeof(size_t);

			malloc_count = 0;
			while (1) {
				transmem_block_t *newmem;
				ICONV_CONST char *inptr = (ICONV_CONST char *) inbuf;
				size_t inleft = resultlen;
				char *outptr = (unsigned char *) outbuf;
				size_t outleft;

				if (freemem_size < sizeof(size_t))
					goto resize_freemem;

				outleft = freemem_size - sizeof(size_t);
				if (iconv(domain->conv, &inptr, &inleft,
					  &outptr, &outleft)
				    != (size_t) (-1)) {
					outbuf = (unsigned char *) outptr;
					break;
				}
				if (errno != E2BIG) {
					goto converted;
				}

resize_freemem:
				/* We must allocate a new buffer or resize the old one.  */
				if (malloc_count > 0) {
					++malloc_count;
					freemem_size =
						malloc_count *
						INITIAL_BLOCK_SIZE;
					newmem = (transmem_block_t *)
						realloc(transmem_list,
							freemem_size);
				} else {
					malloc_count = 1;
					freemem_size = INITIAL_BLOCK_SIZE;
					newmem = (transmem_block_t *)
						malloc(freemem_size);
				}
				if (newmem == NULL) {
					freemem = NULL;
					freemem_size = 0;
					goto converted;
				}
				transmem_list = newmem;
				freemem = newmem;
				outbuf = freemem + sizeof(size_t);
			}

			/* We have now in our buffer a converted string.  Put this
			   into the table of conversions.  */
			*(size_t *) freemem = outbuf - freemem - sizeof(size_t);
			domain->conv_tab[act] = (unsigned char *) freemem;
			/* Shrink freemem, but keep it aligned.  */
			freemem_size -= outbuf - freemem;
			freemem = outbuf;
			freemem += freemem_size & (alignof(size_t) - 1);
			freemem_size = freemem_size & ~(alignof(size_t) - 1);

		}

		/* Now domain->conv_tab[act] contains the translation of all
		   the plural variants.  */
		result = domain->conv_tab[act] + sizeof(size_t);
		resultlen = *(size_t *) domain->conv_tab[act];
	}

converted:
	/* The result string is converted.  */

#endif /* HAVE_ICONV */

	*lengthp = resultlen;
	return result;
}

/* Look up a plural variant.  */
static unsigned char *
plural_lookup(struct loaded_l10nfile *domain, unsigned long int n,
	      const unsigned char *translation, size_t translation_len)
{
	struct loaded_domain *domaindata =
		(struct loaded_domain *) domain->data;
	unsigned long int indexx;
	const unsigned char *p;

	indexx = plural_eval(domaindata->plural, n);
	if (indexx >= domaindata->nplurals)
		/* This should never happen.  It means the plural expression and the
		   given maximum value do not match.  */
		indexx = 0;

	/* Skip INDEX strings at TRANSLATION.  */
	p = translation;
	while (indexx-- > 0) {
		p = strchr(p, '\0');

		/* And skip over the NUL byte.  */
		p++;

		if (p >= translation + translation_len)
			/* This should never happen.  It means the plural expression
			   evaluated to a value larger than the number of variants
			   available for MSGID1.  */
			return (unsigned char *) translation;
	}
	return (unsigned char *) p;
}

/* Function to evaluate the plural expression and return an index value.  */
static unsigned long int
plural_eval(struct expression *pexp, unsigned long int n)
{
	switch (pexp->nargs) {
		case 0:
			switch (pexp->operation) {
				case var:
					return n;
				case num:
					return pexp->val.num;
default:
					break;
			}
			/* NOTREACHED */
			break;
		case 1:
			{
				/* pexp->operation must be lnot.  */
				unsigned long int arg =
					plural_eval(pexp->val.args[0], n);
				return !arg;
			}
		case 2:
			{
				unsigned long int leftarg =
					plural_eval(pexp->val.args[0], n);
				if (pexp->operation == lor)
					return leftarg
						|| plural_eval(pexp->val.
							       args[1], n);
				else if (pexp->operation == land)
					return leftarg
						&& plural_eval(pexp->val.
							       args[1], n);
				else {
					unsigned long int rightarg =
						plural_eval(pexp->val.args[1],
							    n);

					switch (pexp->operation) {
						case mult:
							return leftarg *
								rightarg;
						case divide:
							return leftarg /
								rightarg;
						case module:
							return leftarg %
								rightarg;
						case plus:
							return leftarg +
								rightarg;
						case minus:
							return leftarg -
								rightarg;
						case less_than:
							return leftarg <
								rightarg;
						case greater_than:
							return leftarg >
								rightarg;
						case less_or_equal:
							return leftarg <=
								rightarg;
						case greater_or_equal:
							return leftarg >=
								rightarg;
						case equal:
							return leftarg ==
								rightarg;
						case not_equal:
							return leftarg !=
								rightarg;
default:
							break;
					}
				}
				/* NOTREACHED */
				break;
			}
		case 3:
			{
				/* pexp->operation must be qmop.  */
				unsigned long int boolarg =
					plural_eval(pexp->val.args[0], n);
				return plural_eval(pexp->val.
						   args[boolarg ? 1 : 2], n);
			}
	}
	/* NOTREACHED */
	return 0;
}

/* Return string representation of locale CATEGORY.  */
static const unsigned char *
category_to_name(int category)
{
	const unsigned char *retval;

	switch (category) {
#ifdef LC_COLLATE
		case LC_COLLATE:
			retval = "LC_COLLATE";
			break;
#endif
#ifdef LC_CTYPE
		case LC_CTYPE:
			retval = "LC_CTYPE";
			break;
#endif
#ifdef LC_MONETARY
		case LC_MONETARY:
			retval = "LC_MONETARY";
			break;
#endif
#ifdef LC_NUMERIC
		case LC_NUMERIC:
			retval = "LC_NUMERIC";
			break;
#endif
#ifdef LC_TIME
		case LC_TIME:
			retval = "LC_TIME";
			break;
#endif
#ifdef LC_MESSAGES
		case LC_MESSAGES:
			retval = "LC_MESSAGES";
			break;
#endif
#ifdef LC_RESPONSE
		case LC_RESPONSE:
			retval = "LC_RESPONSE";
			break;
#endif
#ifdef LC_ALL
		case LC_ALL:
			/* This might not make sense but is perhaps better than any other
			   value.  */
			retval = "LC_ALL";
			break;
#endif
default:
			/* If you have a better idea for a default value let me know.  */
			retval = "LC_XXX";
	}

	return retval;
}

/* Guess value of current locale from value of the environment variables.  */
static const unsigned char *
guess_category_value(int category, const unsigned char *categoryname)
{
	const unsigned char *language;
	const unsigned char *retval;

	/* Takes precedence to anything else, damn it's what the application wants!
	 * ;-) --pasky  */
	if (LANGUAGE && *LANGUAGE)
		return LANGUAGE;

	/* The highest priority value is the `LANGUAGE' environment
	   variable.  But we don't use the value if the currently selected
	   locale is the C locale.  This is a GNU extension.  */
	/* XXX: This GNU extension breaks things for me and I can't see what is it
	 * good for - I think it only makes things more difficult and arcane, since
	 * it requires you to set up more variables than LANGUAGE, it's poorly
	 * documented and so on. If this breaks anything, let me know at
	 * pasky@ucw.cz, I'm really curious. If LANGUAGE exists, we just use it and
	 * do no more tests. This is an ELinks extension. --pasky  */
	language = getenv("LANGUAGE");
	if (language && language[0])
		return language;

	/* We have to proceed with the POSIX methods of looking to `LC_ALL',
	   `LC_xxx', and `LANG'.  On some systems this can be done by the
	   `setlocale' function itself.  */
#if (defined HAVE_SETLOCALE && defined HAVE_LC_MESSAGES && defined HAVE_LOCALE_NULL)
	retval = setlocale(category, NULL);
#else
	/* Setting of LC_ALL overwrites all other.  */
	retval = getenv("LC_ALL");
	if (retval == NULL || retval[0] == '\0') {
		/* Next comes the name of the desired category.  */
		retval = getenv(categoryname);
		if (retval == NULL || retval[0] == '\0') {
			/* Last possibility is the LANG environment variable.  */
			retval = getenv("LANG");
			if (retval == NULL || retval[0] == '\0')
				/* We use C as the default domain.  POSIX says this is
				   implementation defined.  */
				retval = "C";
		}
	}
#endif

	return retval;
}
