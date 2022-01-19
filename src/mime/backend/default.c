/* Option system based mime backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/libintl.h"
#include "main/module.h"
#include "mime/backend/common.h"
#include "mime/backend/default.h"
#include "mime/mime.h"
#include "osdep/osdep.h"		/* For get_system_str() */
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static union option_info default_mime_options[] = {
	INIT_OPT_TREE(C_("mime"), N_("MIME type associations"),
		C_("type"), OPT_AUTOCREATE,
		N_("Handler <-> MIME type association. The first sub-tree is "
		"the MIME class while the second sub-tree is the MIME type "
		"(ie. image/gif handler will reside at mime.type.image.gif). "
		"Each MIME type option should contain (case-sensitive) name "
		"of the MIME handler (its properties are stored at "
		"mime.handler.<name>).")),

	INIT_OPT_TREE(C_("mime.type"), NULL,
		C_("_template_"), OPT_AUTOCREATE,
		N_("Handler matching this MIME-type class "
		"('*' is used here in place of '.').")),

	INIT_OPT_STRING(C_("mime.type._template_"), NULL,
		C_("_template_"), OPT_ZERO, "",
		N_("Handler matching this MIME-type name "
		"('*' is used here in place of '.').")),


	INIT_OPT_TREE(C_("mime"), N_("File type handlers"),
		C_("handler"), OPT_AUTOCREATE,
		N_("A file type handler is a set of information about how to "
		"use an external program to view a file. It is possible to "
		"refer to it for several MIME types -- e.g., you can define "
		"an 'image' handler to which mime.type.image.png, "
		"mime.type.image.jpeg, and so on will refer; or one might "
		"define a handler for a more specific type of file -- e.g., "
		"PDF files. Note you must define both a MIME handler "
		"and a MIME type association for it to work.")),

	INIT_OPT_TREE(C_("mime.handler"), NULL,
		C_("_template_"), OPT_AUTOCREATE,
		N_("Description of this handler.")),

	INIT_OPT_TREE(C_("mime.handler._template_"), NULL,
		C_("_template_"), OPT_ZERO,
		N_("System-specific handler description "
		"(ie. unix, unix-xwin, ...).")),

	INIT_OPT_BOOL(C_("mime.handler._template_._template_"), N_("Ask before opening"),
		C_("ask"), OPT_ZERO, 1,
		N_("Ask before opening.")),

	INIT_OPT_BOOL(C_("mime.handler._template_._template_"), N_("Block terminal"),
		C_("block"), OPT_ZERO, 1,
		N_("Block the terminal when the handler is running.")),

	INIT_OPT_STRING(C_("mime.handler._template_._template_"), N_("Program"),
		C_("program"), OPT_ZERO, "",
		/* xgettext:no-c-format */
		N_("External viewer for this file type. "
		"'%f' in this string will be substituted by a file name, "
		"'%u' by its uri. "
		"Do _not_ put single- or double-quotes around the % sign.")),


	INIT_OPT_TREE(C_("mime"), N_("File extension associations"),
		C_("extension"), OPT_AUTOCREATE,
		N_("Extension <-> MIME type association.")),

	INIT_OPT_STRING(C_("mime.extension"), NULL,
		C_("_template_"), OPT_ZERO, "",
		N_("MIME-type matching this file extension "
		"('*' is used here in place of '.').")),

#define INIT_OPT_MIME_EXTENSION(extension, type) \
	INIT_OPT_STRING(C_("mime.extension"), NULL, extension, OPT_ZERO, type, NULL)

	INIT_OPT_MIME_EXTENSION(C_("gif"),		"image/gif"),
	INIT_OPT_MIME_EXTENSION(C_("jpg"),		"image/jpg"),
	INIT_OPT_MIME_EXTENSION(C_("jpeg"),		"image/jpeg"),
	INIT_OPT_MIME_EXTENSION(C_("png"),		"image/png"),
	INIT_OPT_MIME_EXTENSION(C_("txt"),		"text/plain"),
	INIT_OPT_MIME_EXTENSION(C_("htm"),		"text/html"),
	INIT_OPT_MIME_EXTENSION(C_("html"),		"text/html"),
	INIT_OPT_MIME_EXTENSION(C_("gmi"),		"text/gemini"),
#ifdef CONFIG_BITTORRENT
	INIT_OPT_MIME_EXTENSION(C_("torrent"),	"application/x-bittorrent"),
#endif
#ifdef CONFIG_DOM
	INIT_OPT_MIME_EXTENSION(C_("rss"),		"application/rss+xml"),
	INIT_OPT_MIME_EXTENSION(C_("xbel"),		"application/xbel+xml"),
	INIT_OPT_MIME_EXTENSION(C_("sgml"),		"application/docbook+xml"),
#endif

	NULL_OPTION_INFO,
};

static char *
get_content_type_default(char *extension)
{
	struct option *opt_tree;
	struct option *opt;
	char *extend = extension + strlen(extension) - 1;

	if (extend < extension)	return NULL;

	opt_tree = get_opt_rec_real(config_options, "mime.extension");
	assert(opt_tree);

	foreach (opt, *opt_tree->value.tree) {
		char *namepos = opt->name + strlen(opt->name) - 1;
		char *extpos = extend;

		/* Match the longest possible part of URL.. */

#define star2dot(achar)	((achar) == '*' ? '.' : (achar))

		while (extension <= extpos && opt->name <= namepos
		       && *extpos == star2dot(*namepos)) {
			extpos--;
			namepos--;
		}

#undef star2dot

		/* If we matched whole extension and it is really an
		 * extension.. */
		if (namepos < opt->name
		    && (extpos < extension || *extpos == '.'))
			return stracpy(opt->value.string);
	}

	return NULL;
}

static struct option *
get_mime_type_option(char *type)
{
	struct option *opt;
	struct string name;

	opt = get_opt_rec_real(config_options, "mime.type");
	if (!opt) return NULL;

	if (!init_string(&name)) return NULL;

	if (add_optname_to_string(&name, type, strlen(type))) {
		/* Search for end of the base type. */
		char *pos = strchr(name.source, '/');

		if (pos) {
			*pos = '.';
			opt = get_opt_rec_real(opt, name.source);
			done_string(&name);

			return opt;
		}
	}

	done_string(&name);
	return NULL;
}

static inline struct option *
get_mime_handler_option(struct option *type_opt, int xwin)
{
	struct option *handler_opt;

	assert(type_opt);

	handler_opt = get_opt_rec_real(config_options, "mime.handler");
	if (!handler_opt) return NULL;

	handler_opt = get_opt_rec_real(handler_opt, type_opt->value.string);
	if (!handler_opt) return NULL;

	return get_opt_rec_real(handler_opt, get_system_str(xwin));
}

static struct mime_handler *
get_mime_handler_default(char *type, int have_x)
{
	struct option *type_opt = get_mime_type_option(type);
	struct option *handler_opt;

	if (!type_opt) return NULL;

	handler_opt = get_mime_handler_option(type_opt, have_x);
	if (!handler_opt) return NULL;

	return init_mime_handler(get_opt_str_tree(handler_opt, "program", NULL),
				 type_opt->value.string,
				 default_mime_module.name,
				 get_opt_bool_tree(handler_opt, "ask", NULL),
				 get_opt_bool_tree(handler_opt, "block", NULL));
}


const struct mime_backend default_mime_backend = {
	/* get_content_type: */	get_content_type_default,
	/* get_mime_handler: */	get_mime_handler_default,
};

struct module default_mime_module = struct_module(
	/* name: */		N_("Option system"),
	/* options: */		default_mime_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
