/* $Id: common.h,v 1.4 2003/05/08 21:42:57 zas Exp $ */

#ifndef EL__BOOKMARKS_BACKEND_COMMON_H
#define EL__BOOKMARKS_BACKEND_COMMON_H

#include <stdio.h>
#include "util/lists.h"
#include "util/secsave.h"

struct bookmarks_backend {
	/* Order matters here. --Zas. */
	unsigned char *(*filename)(int);
	void (*read)(FILE *);
	void (*write)(struct secure_save_info *, struct list_head *);
};

void bookmarks_read(void);
void bookmarks_write(struct list_head *);

#endif
