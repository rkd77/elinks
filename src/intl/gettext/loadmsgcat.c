/* Load needed message catalogs.
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

/* Modified on 2007-07-02 by Kalle Olavi Niemitalo.  */

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
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#if defined HAVE_UNISTD_H
#include <unistd.h>
#endif


#if (defined HAVE_MMAP && defined HAVE_MUNMAP && !defined DISALLOW_MMAP)
#include <sys/mman.h>
/* Use a custom macro instead of overloading HAVE_MMAP, because the
 * following #include directives may cause "config.h" to be included
 * again (bug 960).  It might be good to remove #include "config.h"
 * directives from header files and keep them in *.c files only, but
 * that seems too risky for the stable elinks-0.11 branch.  */
#undef LOADMSGCAT_USE_MMAP
#define LOADMSGCAT_USE_MMAP	1
#else
#undef LOADMSGCAT_USE_MMAP
#endif

#include "elinks.h"

#include "intl/gettext/gettext.h"
#include "intl/gettext/gettextP.h"
#include "main/main.h"
#include "util/memory.h"
#include "util/string.h"


/* For systems that distinguish between text and binary I/O.
   O_BINARY is usually declared in <fcntl.h>. */
#if !defined O_BINARY && defined _O_BINARY
  /* For MSC-compatible compilers.  */
#define O_BINARY _O_BINARY
#define O_TEXT _O_TEXT
#endif
#ifdef __BEOS__
  /* BeOS 5 has O_BINARY and O_TEXT, but they have no effect.  */
#undef O_BINARY
#undef O_TEXT
#endif
/* On reasonable systems, binary I/O is the default.  */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* We need a sign, whether a new catalog was loaded, which can be associated
   with all translations.  This is important if the translations are
   cached by one of GCC's features.  */
int _nl_msg_cat_cntr;


/* For compilers without support for ISO C 99 struct/union initializers:
   Initialization at run-time.  */

static struct expression plvar;
static struct expression plone;
static struct expression germanic_plural;

static void
init_germanic_plural(void)
{
	if (plone.val.num == 0) {
		plvar.nargs = 0;
		plvar.operation = var;

		plone.nargs = 0;
		plone.operation = num;
		plone.val.num = 1;

		germanic_plural.nargs = 2;
		germanic_plural.operation = not_equal;
		germanic_plural.val.args[0] = &plvar;
		germanic_plural.val.args[1] = &plone;
	}
}

#define INIT_GERMANIC_PLURAL() init_germanic_plural ()

/* Initialize the codeset dependent parts of an opened message catalog.
   Return the header entry.  */
const unsigned char *
_nl_init_domain_conv(struct loaded_l10nfile *domain_file,
		     struct loaded_domain *domain,
		     struct binding *domainbinding)
{
	/* Find out about the character set the file is encoded with.
	   This can be found (in textual form) in the entry "".  If this
	   entry does not exist or if this does not contain the `charset='
	   information, we will assume the charset matches the one the
	   current locale and we don't have to perform any conversion.  */
	unsigned char *nullentry;
	size_t nullentrylen;

	/* Preinitialize fields, to avoid recursion during _nl_find_msg.  */
	domain->codeset_cntr =
		(domainbinding != NULL ? domainbinding->codeset_cntr : 0);
#if HAVE_ICONV
	domain->conv = (iconv_t) - 1;
#endif
	domain->conv_tab = NULL;

	/* Get the header entry.  */
	nullentry = _nl_find_msg(domain_file, domainbinding, "", &nullentrylen);

	if (nullentry != NULL) {
#if HAVE_ICONV
		const unsigned char *charsetstr;

		charsetstr = strstr(nullentry, "charset=");
		if (charsetstr != NULL) {
			size_t len;
			unsigned char *charset;
			const unsigned char *outcharset;

			charsetstr += strlen("charset=");
			len = strcspn(charsetstr, " \t\n");

			charset = (unsigned char *) fmem_alloc(len + 1);
			*((unsigned char *) mempcpy(charset, charsetstr, len)) = '\0';

			/* The output charset should normally be determined by the
			   locale.  But sometimes the locale is not used or not correctly
			   set up, so we provide a possibility for the user to override
			   this.  Moreover, the value specified through
			   bind_textdomain_codeset overrides both.  */
			if (domainbinding != NULL
			    && domainbinding->codeset != NULL)
				outcharset = domainbinding->codeset;
			else {
				outcharset = getenv("OUTPUT_CHARSET");
				if (outcharset == NULL || outcharset[0] == '\0') {
					extern const unsigned char *elinks_locale_charset(void);

					outcharset = elinks_locale_charset();
				}
			}

			/* When using GNU libiconv, we want to use transliteration.  */
#if _LIBICONV_VERSION >= 0x0105
			len = strlen(outcharset);
			{
				unsigned char *tmp = (unsigned char *) fmem_alloc(len + 10 + 1);

				memcpy(tmp, outcharset, len);
				memcpy(tmp + len, "//TRANSLIT", 10 + 1);
				outcharset = tmp;
			}
#endif
			domain->conv = iconv_open(outcharset, charset);
#if _LIBICONV_VERSION >= 0x0105
			fmem_free((void *) outcharset);
#endif

			fmem_free(charset);
		}
#endif /* HAVE_ICONV */
	}

	return nullentry;
}

/* Frees the codeset dependent parts of an opened message catalog.  */
void
_nl_free_domain_conv(struct loaded_domain *domain)
{
	if (domain->conv_tab != NULL && domain->conv_tab != (unsigned char **) -1)
		free(domain->conv_tab);
#if HAVE_ICONV
	if (domain->conv != (iconv_t) - 1)
		iconv_close(domain->conv);
#endif
}

/* We cannot use our memory functions here because of circular library
 * dependencies. */

/* This is hacked for ELinks - we want to look up for the translations at the
 * correct place even if we are being ran from the source/build tree. */
static struct string *
add_filename_to_string(struct string *str, struct loaded_l10nfile *domain_file)
{
	unsigned char *slash = strrchr(program.path, '/');
	size_t dirnamelen = (slash ? slash - program.path + 1 : 0);

	/* Check if elinks is being run from the source tree. */
	if (dirnamelen < 4
	    || strncmp(program.path + dirnamelen - 4, "src", 3))
		return NULL;

	if ((dirnamelen && !add_bytes_to_string(str, program.path, dirnamelen))
	    || !add_to_string(str, "../po/")
	    || !add_bytes_to_string(str,
				    (unsigned char *) domain_file->langdirname,
				    domain_file->langdirnamelen)
	    || !add_to_string(str, ".gmo"))
		return NULL;

	return str;
}

/* Load the message catalogs specified by FILENAME.  If it is no valid
   message catalog do nothing.  */
void
_nl_load_domain(struct loaded_l10nfile *domain_file,
		struct binding *domainbinding)
{
	int fd = -1;
	size_t size;
	struct stat st;
	struct mo_file_header *data = (struct mo_file_header *) -1;
	int use_mmap = 0;
	struct loaded_domain *domain;
	const unsigned char *nullentry;

	domain_file->decided = 1;
	domain_file->data = NULL;

	/* Note that it would be useless to store domainbinding in domain_file
	   because domainbinding might be == NULL now but != NULL later (after
	   a call to bind_textdomain_codeset).  */

	{
		struct string filename;

		if (init_string(&filename)
		    && add_filename_to_string(&filename, domain_file)) {
			fd = open(filename.source, O_RDONLY | O_BINARY);
		}

		done_string(&filename);

		if (fd != -1)
			goto source_success;
	}

	/* If the record does not represent a valid locale the FILENAME
	   might be NULL.  This can happen when according to the given
	   specification the locale file name is different for XPG and CEN
	   syntax.  */
	if (domain_file->filename == NULL)
		return;

	/* Try to open the addressed file.  */
	fd = open(domain_file->filename, O_RDONLY | O_BINARY);
	if (fd == -1)
		return;

source_success:

	/* We must know about the size of the file.  */
	if (fstat(fd, &st) != 0
	    || (size = (size_t) st.st_size) != st.st_size
	    || (size < sizeof(struct mo_file_header))) {
		/* Something went wrong.  */
		close(fd);
		return;
	}
#ifdef LOADMSGCAT_USE_MMAP
	/* Now we are ready to load the file.  If mmap() is available we try
	   this first.  If not available or it failed we try to load it.  */
	data = (struct mo_file_header *) mmap(NULL, size, PROT_READ,
					      MAP_PRIVATE, fd, 0);

	if (data != (struct mo_file_header *) -1) {
		/* mmap() call was successful.  */
		close(fd);
		use_mmap = 1;
	}
#endif

	/* If the data is not yet available (i.e. mmap'ed) we try to load
	   it manually.  */
	if (data == (struct mo_file_header *) -1) {
		size_t to_read;
		unsigned char *read_ptr;

		data = (struct mo_file_header *) malloc(size);
		if (data == NULL)
			return;

		to_read = size;
		read_ptr = (unsigned char *) data;
		do {
			ssize_t nb = safe_read(fd, read_ptr, to_read);

			if (nb <= 0) {
				close(fd);
				return;
			}
			read_ptr += nb;
			to_read -= nb;
		} while (to_read > 0);

		close(fd);
	}

	/* Using the magic number we can test whether it really is a message
	   catalog file.  */
	if (data->magic != _MAGIC && data->magic != _MAGIC_SWAPPED) {
		/* The magic number is wrong: not a message catalog file.  */
#ifdef LOADMSGCAT_USE_MMAP
		if (use_mmap)
			munmap((void *) data, size);
		else
#endif
			free(data);
		return;
	}

	domain = (struct loaded_domain *) malloc(sizeof(struct loaded_domain));
	if (domain == NULL)
		return;
	domain_file->data = domain;

	domain->data = (unsigned char *) data;
	domain->use_mmap = use_mmap;
	domain->mmap_size = size;
	domain->must_swap = data->magic != _MAGIC;

	/* Fill in the information about the available tables.  */
	switch (W(domain->must_swap, data->revision)) {
		case 0:
			domain->nstrings = W(domain->must_swap, data->nstrings);
			domain->orig_tab = (struct string_desc *)
				((unsigned char *) data +
				 W(domain->must_swap, data->orig_tab_offset));
			domain->trans_tab = (struct string_desc *)
				((unsigned char *) data +
				 W(domain->must_swap, data->trans_tab_offset));
			domain->hash_size =
				W(domain->must_swap, data->hash_tab_size);
			domain->hash_tab = (nls_uint32 *)
				((unsigned char *) data +
				 W(domain->must_swap, data->hash_tab_offset));
			break;
default:
			/* This is an invalid revision.  */
#ifdef LOADMSGCAT_USE_MMAP
			if (use_mmap)
				munmap((void *) data, size);
			else
#endif
				free(data);
			free(domain);
			domain_file->data = NULL;
			return;
	}

	/* Now initialize the character set converter from the character set
	   the file is encoded with (found in the header entry) to the domain's
	   specified character set or the locale's character set.  */
	nullentry = _nl_init_domain_conv(domain_file, domain, domainbinding);

	/* Also look for a plural specification.  */
	if (nullentry != NULL) {
		const unsigned char *plural;
		const unsigned char *nplurals;

		plural = strstr(nullentry, "plural=");
		nplurals = strstr(nullentry, "nplurals=");
		if (plural == NULL || nplurals == NULL)
			goto no_plural;
		else {
			/* First get the number.  */
			unsigned char *endp;
			unsigned long int n;
			struct parse_args args;

			nplurals += 9;
			skip_space(nplurals);

			for (endp = (unsigned char *) nplurals, n = 0;
			     isdigit(*endp); endp++)
				n = n * 10 + (*endp - '0');

			domain->nplurals = n;
			if (nplurals == endp)
				goto no_plural;

			/* Due to the restrictions bison imposes onto the interface of the
			   scanner function we have to put the input string and the result
			   passed up from the parser into the same structure which address
			   is passed down to the parser.  */
			plural += 7;
			args.cp = plural;
			if (gettext__parse(&args) != 0)
				goto no_plural;
			domain->plural = args.res;
		}
	} else {
		/* By default we are using the Germanic form: singular form only
		   for `one', the plural form otherwise.  Yes, this is also what
		   English is using since English is a Germanic language.  */
no_plural:
		INIT_GERMANIC_PLURAL();
		domain->plural = &germanic_plural;
		domain->nplurals = 2;
	}
}

#if 0
void
_nl_unload_domain(struct loaded_domain *domain)
{
	if (domain->plural != &germanic_plural)
		__gettext_free_exp(domain->plural);

	_nl_free_domain_conv(domain);

#ifdef _POSIX_MAPPED_FILES
	if (domain->use_mmap)
		munmap((void *) domain->data, domain->mmap_size);
	else
#endif	/* _POSIX_MAPPED_FILES */
		free((void *) domain->data);

	free(domain);
}
#endif
