/* $Id: version.h,v 1.6 2003/10/27 02:17:52 jonas Exp $ */

#ifndef EL__MODULES_VERSION_H
#define EL__MODULES_VERSION_H

struct terminal;

unsigned char *get_dyn_full_version(struct terminal *term, int more);
void init_static_version(void);

#endif /* EL__MODULES_VERSION_H */
