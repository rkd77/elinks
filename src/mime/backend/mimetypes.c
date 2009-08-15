/* Support for mime.types files for mapping file extensions to content types */

/* Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 2003-2004 The ELinks Project */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "mime/backend/common.h"
#include "mime/backend/mimetypes.h"
#include "mime/mime.h"
#include "session/session.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memory.h"

#define BACKEND_NAME	"mimetypes"

struct mimetypes_entry {
	unsigned char *content_type;
	unsigned char extension[1];
};

enum mimetypes_option {
	MIMETYPES_TREE,

	MIMETYPES_ENABLE,
	MIMETYPES_PATH,

	MIMETYPES_OPTIONS
};

/* Keep options in alphabetical order. */
static union option_info mimetypes_options[] = {
	INIT_OPT_TREE("mime", N_("Mimetypes files"),
		"mimetypes", 0,
		N_("Options for the support of mime.types files. These files "
		"can be used to find the content type of a URL by looking at "
		"the extension of the file name.")),

	INIT_OPT_BOOL("mime.mimetypes", N_("Enable"),
		"enable", 0, 1,
		N_("Enable mime.types support.")),

	INIT_OPT_STRING("mime.mimetypes", N_("Path"),
		"path", 0, DEFAULT_MIMETYPES_PATH,
		N_("The search path for mime.types files. "
		"Colon-separated list of files.")),

	NULL_OPTION_INFO,
};

#define get_opt_mimetypes(which)		mimetypes_options[(which)].option
#define get_mimetypes(which)		get_opt_mimetypes(which).value
#define get_mimetypes_enable()		get_mimetypes(MIMETYPES_ENABLE).number
#define get_mimetypes_path()		get_mimetypes(MIMETYPES_PATH).string

/* State variables */
static struct hash *mimetypes_map = NULL;


static void
done_mimetypes_entry(struct mimetypes_entry *entry)
{
	if (!entry) return;
	mem_free_if(entry->content_type);
	mem_free(entry);
}

/* Parsing of a mime.types file with the format:
 *
 *	basetype/subtype	extension1 [extension2 ... extensionN]
 *
 * Comments starts with '#'. */

static inline void
parse_mimetypes_extensions(unsigned char *token, unsigned char *ctype)
{
	int ctypelen = strlen(ctype);

	/* Cycle through the file extensions */
	while (*token) {
		struct mimetypes_entry *entry;
		unsigned char *extension;
		struct hash_item *item;
		int extlen;

		skip_space(token);

		extension = token;
		skip_nonspace(token);

		if (!*token) break;
		*token++ = '\0';

		extlen = strlen(extension);
		/* First check if the type is already known. If it is
		 * drop it. This way first files are priotized. */
		item = get_hash_item(mimetypes_map, extension, extlen);
		if (item) continue;

		entry = mem_calloc(1, sizeof(*entry) + extlen);
		if (!entry) continue;

		entry->content_type = memacpy(ctype, ctypelen);
		if (!entry->content_type) {
			done_mimetypes_entry(entry);
			continue;
		}

		memcpy(entry->extension, extension, extlen);

		item = add_hash_item(mimetypes_map, entry->extension, extlen,
				     entry);
		if (!item)
			done_mimetypes_entry(entry);
	}
}

static void
parse_mimetypes_file(unsigned char *filename)
{
	FILE *file = fopen(filename, "rb");
	unsigned char line[MAX_STR_LEN];

	if (!file) return;

	while (fgets(line, MAX_STR_LEN - 1, file)) {
		unsigned char *ctype = line;
		unsigned char *token;

		/* Weed out any comments */
		token = strchr(line, '#');
		if (token)
			*token = '\0';

		skip_space(ctype);

		/* Position on the next field in this line */
		token = ctype;
		skip_nonspace(token);

		if (!*token) continue;
		*token++ = '\0';

		/* Check if malformed content type */
		if (!strchr(ctype, '/')) continue;

		parse_mimetypes_extensions(token, ctype);
	}

	fclose(file);
}

static struct hash *
init_mimetypes_map(void)
{
	unsigned char *path;

	mimetypes_map = init_hash8();
	if (!mimetypes_map)
		return NULL;

	/* Determine the path  */
	path = get_mimetypes_path();
	if (!path || !*path) path = DEFAULT_MIMETYPES_PATH;

	while (*path) {
		unsigned char *filename = get_next_path_filename(&path, ':');

		if (!filename) continue;
		parse_mimetypes_file(filename);
		mem_free(filename);
	}

	return mimetypes_map;
}

static void
done_mimetypes(struct module *module)
{
	struct hash_item *item;
	int i;

	if (!mimetypes_map)
		return;

	foreach_hash_item (item, *mimetypes_map, i) {
		if (item->value) {
			struct mimetypes_entry *entry = item->value;

			done_mimetypes_entry(entry);
		}
	}

	free_hash(&mimetypes_map);
}

static int
change_hook_mimetypes(struct session *ses, struct option *current, struct option *changed)
{
	if (changed == &get_opt_mimetypes(MIMETYPES_PATH)
	    || (changed == &get_opt_mimetypes(MIMETYPES_ENABLE)
		&& !get_mimetypes_enable())) {
		done_mimetypes(&mimetypes_mime_module);
	}

	return 0;
}

static void
init_mimetypes(struct module *module)
{
	static const struct change_hook_info mimetypes_change_hooks[] = {
		{ "mime.mimetypes",		change_hook_mimetypes },
		{ NULL,				NULL },
	};

	register_change_hooks(mimetypes_change_hooks);

	if (get_cmd_opt_bool("anonymous"))
		get_mimetypes_enable() = 0;
}


static unsigned char *
get_content_type_mimetypes(unsigned char *extension)
{
	struct hash_item *item;
	int extensionlen;

	if (!get_mimetypes_enable()
	    || (!mimetypes_map && !init_mimetypes_map()))
		return NULL;

	extension++; /* Skip the leading '.' */
	extensionlen = strlen(extension);
	while (extensionlen) {
		unsigned char *trimmed;

		/* First the given type is looked up. */
		item = get_hash_item(mimetypes_map, extension, extensionlen);

		/* Check list of entries */
		if (item && item->value) {
			struct mimetypes_entry *entry = item->value;

			return stracpy(entry->content_type);
		}

		/* Try to trim the extension from the left. */
		trimmed = strchr(extension, '.');
		if (!trimmed)
			break;

		extensionlen -= trimmed - extension + 1;
		extension = trimmed + 1;
	}

	return NULL;
}

const struct mime_backend mimetypes_mime_backend = {
	/* get_content_type: */	get_content_type_mimetypes,
	/* get_mime_handler: */	NULL,
};

struct module mimetypes_mime_module = struct_module(
	/* name: */		N_("Mimetypes files"),
	/* options: */		mimetypes_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_mimetypes,
	/* done: */		done_mimetypes
);
