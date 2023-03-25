/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

/**
 * \file
 * Useful interned string tables implementation.
 */

#include <libcss/libcss.h>
#include <dom/dom.h>

#include "document/libdom/corestrings.h"
//#include "utils/nsurl.h"
//#include "utils/utils.h"

/* define corestrings */
#define CORESTRING_LWC_VALUE(NAME,VALUE) lwc_string *corestring_lwc_##NAME
#define CORESTRING_DOM_VALUE(NAME,VALUE) dom_string *corestring_dom_##NAME
//#define CORESTRING_NSURL(NAME,VALUE) nsurl *corestring_nsurl_##NAME
#include "document/libdom/corestringlist.h"
#undef CORESTRING_LWC_VALUE
#undef CORESTRING_DOM_VALUE
//#undef CORESTRING_NSURL

/* exported interface documented in utils/corestrings.h */
int corestrings_fini(void)
{
#define CORESTRING_LWC_VALUE(NAME,VALUE)				\
	do {								\
		if (corestring_lwc_##NAME != NULL) {			\
			lwc_string_unref(corestring_lwc_##NAME);	\
			corestring_lwc_##NAME = NULL;			\
		}							\
	} while (0)

#define CORESTRING_DOM_VALUE(NAME,VALUE)				\
	do {								\
		if (corestring_dom_##NAME != NULL) {			\
			dom_string_unref(corestring_dom_##NAME);	\
			corestring_dom_##NAME = NULL;			\
		}							\
	} while (0)

#if 0
#define CORESTRING_NSURL(NAME,VALUE)					\
	do {								\
		if (corestring_nsurl_##NAME != NULL) {			\
			nsurl_unref(corestring_nsurl_##NAME);		\
			corestring_nsurl_##NAME = NULL;			\
		}							\
	} while (0)
#endif

#include "document/libdom/corestringlist.h"

#undef CORESTRING_LWC_VALUE
#undef CORESTRING_DOM_VALUE
//#undef CORESTRING_NSURL

	return 0;//NSERROR_OK;
}


/* exported interface documented in utils/corestrings.h */
int corestrings_init(void)
{
	lwc_error lerror;
	css_error error;
	dom_exception exc;

#define CORESTRING_LWC_VALUE(NAME,VALUE)				\
	do {								\
		lerror = lwc_intern_string(				\
			(const char *)VALUE,				\
			sizeof(VALUE) - 1,				\
			&corestring_lwc_##NAME );			\
		if ((lerror != lwc_error_ok) ||				\
		    (corestring_lwc_##NAME == NULL)) {			\
			error = CSS_NOMEM;				\
			goto error;					\
		}							\
	} while(0)

#define CORESTRING_DOM_VALUE(NAME,VALUE)				\
	do {								\
		exc = dom_string_create_interned(			\
				(const uint8_t *)VALUE,			\
				sizeof(VALUE) - 1,			\
				&corestring_dom_##NAME );		\
		if ((exc != DOM_NO_ERR) ||				\
				(corestring_dom_##NAME == NULL)) {	\
			error = CSS_NOMEM;				\
			goto error;					\
		}							\
	} while(0)

#if 0
#define CORESTRING_NSURL(NAME,VALUE)					\
	do {								\
		error = nsurl_create(VALUE,				\
				     &corestring_nsurl_##NAME);		\
		if (error != NSERROR_OK) {				\
			goto error;					\
		}							\
	} while(0)
#endif

#include "document/libdom/corestringlist.h"

#undef CORESTRING_LWC_VALUE
#undef CORESTRING_DOM_VALUE
//#undef CORESTRING_NSURL

	return 0;//NSERROR_OK;

error:
	corestrings_fini();

	return error;
}
