/* Implementation of the dngettext(3) function.
   Copyright (C) 1995-1997, 2000, 2001 Free Software Foundation, Inc.

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

#include "elinks.h"

#include "intl/gettext/gettextP.h"
#include "intl/gettext/libgnuintl.h"

/* Look up MSGID in the DOMAINNAME message catalog of the current
   LC_MESSAGES locale and skip message according to the plural form.  */
unsigned char *
dngettext__(const unsigned char *domainname, const unsigned char *msgid1,
	    const unsigned char *msgid2, unsigned long int n)
{
	return dcngettext__(domainname, msgid1, msgid2, n, LC_MESSAGES);
}
