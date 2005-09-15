/* Internal bookmarks support - file format backends multiplexing */
/* $Id: common.c,v 1.20.4.1 2005/05/01 22:03:22 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/backend/common.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* Backends dynamic area: */

#include "bookmarks/backend/default.h"
#include "bookmarks/backend/xbel.h"

/* Note that the numbering is static, that means that you have to provide at
 * least dummy NULL handlers even when no support is compiled in. */

static struct bookmarks_backend *bookmarks_backends[] = {
	&default_bookmarks_backend,
#ifdef CONFIG_XBEL_BOOKMARKS
	&xbel_bookmarks_backend,
#else
	NULL,
#endif
};


/* Loads the bookmarks from file */
void
bookmarks_read(void)
{
	int backend = get_opt_int("bookmarks.file_format");
	unsigned char *file_name;
	FILE *f;

	if (!bookmarks_backends[backend]
	    || !bookmarks_backends[backend]->read
	    || !bookmarks_backends[backend]->filename) return;

	file_name = bookmarks_backends[backend]->filename(0);
	if (!file_name) return;
	if (elinks_home) {
		file_name = straconcat(elinks_home, file_name, NULL);
		if (!file_name) return;
	}

	f = fopen(file_name, "rb");
	if (elinks_home) mem_free(file_name);
	if (!f) return;

	bookmarks_backends[backend]->read(f);

	fclose(f);
	bookmarks_unset_dirty();
}

void
bookmarks_write(struct list_head *bookmarks_list)
{
	int backend = get_opt_int("bookmarks.file_format");
	struct secure_save_info *ssi;
	unsigned char *file_name;

	if (!bookmarks_are_dirty()) return;
	if (!bookmarks_backends[backend]
	    || !bookmarks_backends[backend]->write
	    || !elinks_home
	    || !bookmarks_backends[backend]->filename) return;

	/* We do this two-passes because we want backend to possibly decide to
	 * return NULL if it's not suitable to save the bookmarks (otherwise
	 * they would be just truncated to zero by secure_open()). */
	file_name = bookmarks_backends[backend]->filename(1);
	if (!file_name) return;
	file_name = straconcat(elinks_home, file_name, NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177);
	mem_free(file_name);
	if (!ssi) return;

	bookmarks_backends[backend]->write(ssi, bookmarks_list);

	if (!secure_close(ssi)) bookmarks_unset_dirty();
}
