/* Implementation of the bindtextdomain(3) function
   Copyright (C) 1995-1998, 2000, 2001 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "intl/gettext/gettextP.h"
#include "util/string.h"

/* Contains the default location of the message catalogs.  */
extern const unsigned char _nl_default_dirname__[];

/* List with bindings of specific domains.  */
extern struct binding *_nl_domain_bindings__;

/* Prototypes for local functions.  */
static void set_binding_values(const unsigned char *domainname,
			       const unsigned char **dirnamep,
			       const unsigned char **codesetp);

/* Specifies the directory name *DIRNAMEP and the output codeset *CODESETP
   to be used for the DOMAINNAME message catalog.
   If *DIRNAMEP or *CODESETP is NULL, the corresponding attribute is not
   modified, only the current value is returned.
   If DIRNAMEP or CODESETP is NULL, the corresponding attribute is neither
   modified nor returned.  */
static void
set_binding_values(const unsigned char *domainname,
		   const unsigned char **dirnamep,
		   const unsigned char **codesetp)
{
	struct binding *binding;
	int modified;

	/* Some sanity checks.  */
	if (domainname == NULL || domainname[0] == '\0') {
		if (dirnamep)
			*dirnamep = NULL;
		if (codesetp)
			*codesetp = NULL;
		return;
	}

	modified = 0;

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

	if (binding != NULL) {
		if (dirnamep) {
			const unsigned char *dirname = *dirnamep;

			if (dirname == NULL)
				/* The current binding has be to returned.  */
				*dirnamep = binding->dirname;
			else {
				/* The domain is already bound.  If the new value and the old
				   one are equal we simply do nothing.  Otherwise replace the
				   old binding.  */
				unsigned char *result = binding->dirname;

				if (strcmp(dirname, result) != 0) {
					if (strcmp(dirname, _nl_default_dirname__)
					    == 0)
						result = (unsigned char *)
							_nl_default_dirname__;
					else {
						result = strdup(dirname);
					}

					if (result != NULL) {
						if (binding->dirname !=
						    _nl_default_dirname__)
							free(binding->dirname);

						binding->dirname = result;
						modified = 1;
					}
				}
				*dirnamep = result;
			}
		}

		if (codesetp) {
			const unsigned char *codeset = *codesetp;

			if (codeset == NULL)
				/* The current binding has be to returned.  */
				*codesetp = binding->codeset;
			else {
				/* The domain is already bound.  If the new value and the old
				   one are equal we simply do nothing.  Otherwise replace the
				   old binding.  */
				unsigned char *result = binding->codeset;

				if (result == NULL
				    || strcmp(codeset, result) != 0) {
					result = strdup(codeset);

					if (result != NULL) {
						if (binding->codeset != NULL)
							free(binding->codeset);

						binding->codeset = result;
						binding->codeset_cntr++;
						modified = 1;
					}
				}
				*codesetp = result;
			}
		}
	} else if ((dirnamep == NULL || *dirnamep == NULL)
		   && (codesetp == NULL || *codesetp == NULL)) {
		/* Simply return the default values.  */
		if (dirnamep)
			*dirnamep = _nl_default_dirname__;
		if (codesetp)
			*codesetp = NULL;
	} else {
		/* We have to create a new binding.  */
		size_t len = strlen(domainname) + 1;
		struct binding *new_binding =
			(struct binding *)
			malloc(offsetof(struct binding, domainname) + len);

		if (new_binding == NULL)
			goto failed;

		memcpy(new_binding->domainname, domainname, len);

		if (dirnamep) {
			const unsigned char *dirname = *dirnamep;

			if (dirname == NULL)
				/* The default value.  */
				dirname = _nl_default_dirname__;
			else {
				if (strcmp(dirname, _nl_default_dirname__) == 0)
					dirname = _nl_default_dirname__;
				else {
					unsigned char *result;

					result = strdup(dirname);
					if (result == NULL)
						goto failed_dirname;
					dirname = result;
				}
			}
			*dirnamep = dirname;
			new_binding->dirname = (unsigned char *) dirname;
		} else
			/* The default value.  */
			new_binding->dirname = (unsigned char *) _nl_default_dirname__;

		new_binding->codeset_cntr = 0;

		if (codesetp) {
			const unsigned char *codeset = *codesetp;

			if (codeset != NULL) {
				unsigned char *result;

				result = strdup(codeset);
				if (result == NULL)
					goto failed_codeset;
				codeset = result;
				new_binding->codeset_cntr++;
			}
			*codesetp = codeset;
			new_binding->codeset = (unsigned char *) codeset;
		} else
			new_binding->codeset = NULL;

		/* Now enqueue it.  */
		if (_nl_domain_bindings__ == NULL
		    || strcmp(domainname, _nl_domain_bindings__->domainname) < 0)
		{
			new_binding->next = _nl_domain_bindings__;
			_nl_domain_bindings__ = new_binding;
		} else {
			binding = _nl_domain_bindings__;
			while (binding->next != NULL
			       && strcmp(domainname,
					 binding->next->domainname) > 0)
				binding = binding->next;

			new_binding->next = binding->next;
			binding->next = new_binding;
		}

		modified = 1;

		/* Here we deal with memory allocation failures.  */
		if (0) {
failed_codeset:
			if (new_binding->dirname != _nl_default_dirname__)
				free(new_binding->dirname);
failed_dirname:
			free(new_binding);
failed:
			if (dirnamep)
				*dirnamep = NULL;
			if (codesetp)
				*codesetp = NULL;
		}
	}

	/* If we modified any binding, we flush the caches.  */
	if (modified)
		++_nl_msg_cat_cntr;

}

/* Specify that the DOMAINNAME message catalog will be found
   in DIRNAME rather than in the system locale data base.  */
unsigned char *
bindtextdomain__(const unsigned char *domainname, const unsigned char *dirname)
{
	set_binding_values(domainname, &dirname, NULL);
	return (unsigned char *) dirname;
}

/* Specify the character encoding in which the messages from the
   DOMAINNAME message catalog will be returned.  */
unsigned char *
bind_textdomain_codeset__(const unsigned char *domainname, const unsigned char *codeset)
{
	set_binding_values(domainname, NULL, &codeset);
	return (unsigned char *) codeset;
}
