/* $Id: error.h,v 1.6 2003/11/29 21:55:33 jonas Exp $ */

#ifndef EL__SCHED_ERROR_H
#define EL__SCHED_ERROR_H

struct terminal;

unsigned char *get_err_msg(int state, struct terminal *term);

void free_strerror_buf(void);

#endif
