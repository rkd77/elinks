/* Version numbers etc; always rebuilt to reflect the latest info */
/* $Id: vernum.c,v 1.1 2004/04/23 12:29:19 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "vernum.h"

unsigned char *build_date = __DATE__;
unsigned char *build_time = __TIME__;
unsigned char *build_id = BUILD_ID;
