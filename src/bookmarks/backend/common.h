
#ifndef EL__BOOKMARKS_BACKEND_COMMON_H
#define EL__BOOKMARKS_BACKEND_COMMON_H

#include <stdio.h>
#include "util/lists.h"
#include "util/secsave.h"

struct bookmarks_backend {
	/* Order matters here. --Zas. */
	unsigned char *(*filename)(int);
	void (*read)(FILE *);
	void (*write)(struct secure_save_info *, LIST_OF(struct bookmark) *);
};

void bookmarks_read(void);
void bookmarks_write(LIST_OF(struct bookmark) *);

#endif
