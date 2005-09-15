/* Implementation of the textdomain(3) function.
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

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "intl/gettext/gettextP.h"
#include "util/string.h"

/* Name of the default text domain.  */
extern const unsigned char _nl_default_default_domain__[];

/* Default text domain in which entries for gettext(3) are to be found.  */
extern const unsigned char *_nl_current_default_domain__;

/* Set the current default message catalog to DOMAINNAME.
   If DOMAINNAME is null, return the current default.
   If DOMAINNAME is "", reset to the default of "messages".  */
unsigned char *
textdomain__(const unsigned char *domainname)
{
	unsigned char *new_domain;
	unsigned char *old_domain;

	/* A NULL pointer requests the current setting.  */
	if (domainname == NULL)
		return (unsigned char *) _nl_current_default_domain__;

	old_domain = (unsigned char *) _nl_current_default_domain__;

	/* If domain name is the null string set to default domain "messages".  */
	if (domainname[0] == '\0'
	    || strcmp(domainname, _nl_default_default_domain__) == 0) {
		_nl_current_default_domain__ = _nl_default_default_domain__;
		new_domain = (unsigned char *) _nl_current_default_domain__;
	} else if (strcmp(domainname, old_domain) == 0)
		/* This can happen and people will use it to signal that some
		   environment variable changed.  */
		new_domain = old_domain;
	else {
		/* If the following malloc fails `_nl_current_default_domain__'
		   will be NULL.  This value will be returned and so signals we
		   are out of core.  */
		new_domain = strdup(domainname);

		if (new_domain != NULL)
			_nl_current_default_domain__ = new_domain;
	}

	/* We use this possibility to signal a change of the loaded catalogs
	   since this is most likely the case and there is no other easy we
	   to do it.  Do it only when the call was successful.  */
	if (new_domain != NULL) {
		++_nl_msg_cat_cntr;

		if (old_domain != new_domain
		    && old_domain != _nl_default_default_domain__)
			free(old_domain);
	}

	return new_domain;
}
