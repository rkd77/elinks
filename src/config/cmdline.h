/* $Id: cmdline.h,v 1.7 2005/06/13 00:43:27 jonas Exp $ */

#ifndef EL__CONFIG_CMDLINE_H
#define EL__CONFIG_CMDLINE_H

#include "main/main.h"
#include "util/lists.h"

enum retval parse_options(int, unsigned char *[], struct list_head *);

#endif
