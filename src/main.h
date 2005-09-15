/* $Id: main.h,v 1.19.6.1 2005/05/02 20:34:58 jonas Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

enum retval {
	RET_OK,
	RET_ERROR,
	RET_SIGNAL,
	RET_SYNTAX,
	RET_FATAL,
	RET_PING,
	RET_REMOTE,
	RET_COMMAND,
};

extern enum retval retval;
extern int terminate;
extern unsigned char *path_to_exe;

void shrink_memory(int);

#endif
