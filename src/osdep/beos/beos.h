/* $Id: beos.h,v 1.7 2004/08/14 23:08:24 jonas Exp $ */

#ifndef EL__OSDEP_BEOS_BEOS_H
#define EL__OSDEP_BEOS_BEOS_H

#ifdef CONFIG_BEOS

struct terminal;

void open_in_new_be(struct terminal *term, unsigned char *exe_name,
		    unsigned char *param);

#endif

#endif
