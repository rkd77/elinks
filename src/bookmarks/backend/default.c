/* Internal bookmarks support - default file format backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
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
	const int file_cp = get_cp_index("System");

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
			last_bm = add_bookmark_cp(root, 1, file_cp,
						  title, url);
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

struct write_bookmarks_default
{
	struct secure_save_info *ssi;
	int save_folder_state;
	int codepage;
	struct conv_table *conv_table;
};

static void
write_bookmarks_default_inner(const struct write_bookmarks_default *out,
			      LIST_OF(struct bookmark) *bookmarks_list)
{
	struct bookmark *bm;

	foreach (bm, *bookmarks_list) {
		unsigned char *title, *url;

		title = convert_string(out->conv_table, bm->title,
				       strlen(bm->title), out->codepage,
				       CSM_NONE, NULL, NULL, NULL);
		url = convert_string(out->conv_table, bm->url,
				     strlen(bm->url), out->codepage,
				     CSM_NONE, NULL, NULL, NULL);
		secure_fprintf(out->ssi, "%s\t%s\t%d\t",
			       empty_string_or_(title), empty_string_or_(url),
			       bm->box_item->depth);
		if (bm->box_item->type == BI_FOLDER) {
			secure_fputc(out->ssi, 'F');
			if (out->save_folder_state && bm->box_item->expanded)
				secure_fputc(out->ssi, 'E');
		}
		secure_fputc(out->ssi, '\n');

		if (!title || !url) {
			secsave_errno = SS_ERR_OTHER;
			out->ssi->err = ENOMEM;
		}
		mem_free_if(title);
		mem_free_if(url);
		if (out->ssi->err) break;

		if (!list_empty(bm->child))
			write_bookmarks_default_inner(out, &bm->child);
	}
}

/* Saves the bookmarks to file */
static void
write_bookmarks_default(struct secure_save_info *ssi,
		        LIST_OF(struct bookmark) *bookmarks_list)
{
	struct write_bookmarks_default out;

	out.ssi = ssi;
	out.save_folder_state = get_opt_bool("bookmarks.folder_state");
	out.codepage = get_cp_index("System");
	out.conv_table = get_translation_table(get_cp_index("UTF-8"),
					       out.codepage);
	write_bookmarks_default_inner(&out, bookmarks_list);
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
