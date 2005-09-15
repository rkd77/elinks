/* $Id: os2.h,v 1.8 2004/08/14 23:19:00 jonas Exp $ */

#ifndef EL__OSDEP_OS2_OS2_H
#define EL__OSDEP_OS2_OS2_H

#ifdef CONFIG_OS2

struct terminal;

void open_in_new_vio(struct terminal *term, unsigned char *exe_name,
		     unsigned char *param);
void open_in_new_fullscreen(struct terminal *term, unsigned char *exe_name,
			    unsigned char *param);

#endif

#endif
