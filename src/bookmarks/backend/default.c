/* Internal bookmarks support - default file format backend */

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
#include "bookmarks/backend/default.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"

#define BOOKMARKS_FILENAME		"bookmarks"


/* Loads the bookmarks from file */
static void
read_bookmarks_default(FILE *f)
{
	/* INBUF_SIZE = max. title length + 1 byte for separator
	 * + max. url length + 1 byte for separator + 5 bytes for depth
	 * + 1 byte for end of line + 1 byte for null char + reserve */
#define INBUF_SIZE ((MAX_STR_LEN - 1) + 1 + (MAX_STR_LEN - 1) + 1 + 5 + 1 + 1 \
		    + MAX_STR_LEN)
	unsigned char in_buffer[INBUF_SIZE]; /* read buffer */
	struct bookmark *last_bm = NULL;
	int last_depth = 0;

	/* TODO: Ignore lines with bad chars in title or url (?). -- Zas */
	while (fgets(in_buffer, INBUF_SIZE, f)) {
		unsigned char *title = in_buffer;
		unsigned char *url;
		unsigned char *depth_str;
		int depth = 0;
		unsigned char *flags = NULL;
		unsigned char *line_end;

		/* Load URL. */

		url = strchr(in_buffer, '\t');

		/* If separator is not found, or title is empty or too long,
		 * skip that line. */
		if (!url || url == in_buffer
		    || url - in_buffer > MAX_STR_LEN - 1)
			continue;
		*url = '\0';
		url++;

		/* Load depth. */

		depth_str = strchr(url, '\t');
		if (depth_str && depth_str - url > MAX_STR_LEN - 1)
			continue;

		if (depth_str) {
			*depth_str = '\0';
			depth_str++;
			depth = atoi(depth_str);
			int_bounds(&depth, 0, last_bm ? last_depth + 1 : 0);

			/* Load flags. */

			flags = strchr(depth_str, '\t');
			if (flags) {
				*flags = '\0';
				flags++;
			}
		} else {
			depth_str = url;
		}

		/* Load EOLN. */

		line_end = strchr(flags ? flags : depth_str, '\n');
		if (!line_end)
			continue;
		*line_end = '\0';

		{
			struct bookmark *root = NULL;

			if (depth > 0) {
				if (depth == last_depth) {
					root = last_bm->root;
				} else if (depth > last_depth) {
					root = last_bm;
				} else {
					while (last_depth - depth) {
						last_bm = last_bm->root;
						last_depth--;
					}
					root = last_bm->root;
				}
			}
			last_bm = add_bookmark(root, 1, title, url);
			last_depth = depth;

			if (!*url && title[0] == '-' && title[1] == '\0') {
			       	last_bm->box_item->type = BI_SEPARATOR;

			} else {
				while (flags && *flags && last_bm) {
					switch (*flags) {
						case 'F':
							last_bm->box_item->type = BI_FOLDER;
							break;
						case 'E':
							last_bm->box_item->expanded = 1;
							break;
					}
					flags++;
				}
			}
		}
	}
#undef INBUF_SIZE
}

/* Saves the bookmarks to file */
static void
write_bookmarks_default(struct secure_save_info *ssi,
		        LIST_OF(struct bookmark) *bookmarks_list)
{
	int folder_state = get_opt_bool("bookmarks.folder_state");
	struct bookmark *bm;

	foreach (bm, *bookmarks_list) {
		/** @todo Bug 153: bm->title should be UTF-8.
		 * @todo Bug 1066: bm->url should be UTF-8.  */
		secure_fprintf(ssi, "%s\t%s\t%d\t", bm->title, bm->url, bm->box_item->depth);
		if (bm->box_item->type == BI_FOLDER) {
			secure_fputc(ssi, 'F');
			if (folder_state && bm->box_item->expanded)
				secure_fputc(ssi, 'E');
		}
		secure_fputc(ssi, '\n');
		if (ssi->err) break;

		if (!list_empty(bm->child))
			write_bookmarks_default(ssi, &bm->child);
	}
}

static unsigned char *
filename_bookmarks_default(int writing)
{
	return BOOKMARKS_FILENAME;
}


struct bookmarks_backend default_bookmarks_backend = {
	filename_bookmarks_default,
	read_bookmarks_default,
	write_bookmarks_default,
};
