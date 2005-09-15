/* $Id: cmdline.h,v 1.5.6.1 2005/05/02 20:34:59 jonas Exp $ */

#ifndef EL__CONFIG_CMDLINE_H
#define EL__CONFIG_CMDLINE_H

#include "main.h"
#include "util/lists.h"

enum retval parse_options(int, unsigned char *[], struct list_head *);

#endif
