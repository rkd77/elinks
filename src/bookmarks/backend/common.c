/* Internal bookmarks support - file format backends multiplexing */

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
#include "config/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* Backends dynamic area: */

#include "bookmarks/backend/default.h"
#include "bookmarks/backend/xbel.h"

/* Note that the numbering is static, that means that you have to provide at
 * least dummy NULL handlers even when no support is compiled in. */
/* XXX: keep original order since we use bookmarks.file_format option value
 * as index. So it means you should add any new backend just before the
 * ending NULL. */
static struct bookmarks_backend *bookmarks_backends[] = {
	&default_bookmarks_backend,
#ifdef CONFIG_XBEL_BOOKMARKS
	&xbel_bookmarks_backend,
#else
	NULL,
#endif
};


static int loaded_backend_num = -1;

/* Loads the bookmarks from file */
void
bookmarks_read(void)
{
	int backend_num = get_opt_int("bookmarks.file_format", NULL);
	struct bookmarks_backend *backend = bookmarks_backends[backend_num];
	unsigned char *file_name;
	FILE *f;

	if (!backend
	    || !backend->read
	    || !backend->filename) return;

	file_name = backend->filename(0);
	if (!file_name) return;
	if (elinks_home) {
		file_name = straconcat(elinks_home, file_name,
				       (unsigned char *) NULL);
		if (!file_name) return;
	}

	f = fopen(file_name, "rb");
	if (elinks_home) mem_free(file_name);
	if (!f) return;

	backend->read(f);

	fclose(f);
	bookmarks_unset_dirty();
	loaded_backend_num = backend_num;
}

void
bookmarks_write(LIST_OF(struct bookmark) *bookmarks_list)
{
	int backend_num = get_opt_int("bookmarks.file_format", NULL);
	struct bookmarks_backend *backend = bookmarks_backends[backend_num];
	struct secure_save_info *ssi;
	unsigned char *file_name;

	if (!bookmarks_are_dirty() && backend_num == loaded_backend_num) return;
	if (!backend
	    || !backend->write
	    || !elinks_home
	    || !backend->filename) return;

	/* We do this two-passes because we want backend to possibly decide to
	 * return NULL if it's not suitable to save the bookmarks (otherwise
	 * they would be just truncated to zero by secure_open()). */
	file_name = backend->filename(1);
	if (!file_name) return;
	file_name = straconcat(elinks_home, file_name, (unsigned char *) NULL);
	if (!file_name) return;

	ssi = secure_open(file_name);
	mem_free(file_name);
	if (!ssi) return;

	backend->write(ssi, bookmarks_list);

	if (!secure_close(ssi)) bookmarks_unset_dirty();
}
