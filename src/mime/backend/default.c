/* Option system based mime backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "mime/backend/common.h"
#include "mime/backend/default.h"
#include "mime/mime.h"
#include "osdep/osdep.h"		/* For get_system_str() */
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static struct option_info default_mime_options[] = {
	INIT_OPT_TREE("mime", N_("MIME type associations"),
		"type", OPT_AUTOCREATE,
		N_("Handler <-> MIME type association. The first sub-tree is "
		"the MIME class while the second sub-tree is the MIME type "
		"(ie. image/gif handler will reside at mime.type.image.gif). "
		"Each MIME type option should contain (case-sensitive) name "
		"of the MIME handler (its properties are stored at "
		"mime.handler.<name>).")),

	INIT_OPT_TREE("mime.type", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Handler matching this MIME-type class "
		"('*' is used here in place of '.').")),

	INIT_OPT_STRING("mime.type._template_", NULL,
		"_template_", 0, "",
		N_("Handler matching this MIME-type name "
		"('*' is used here in place of '.').")),


	INIT_OPT_TREE("mime", N_("File type handlers"),
		"handler", OPT_AUTOCREATE,
		N_("A file type handler is a set of information about how to "
		"use an external program to view a file. It is possible to "
		"refer to it for several MIME types -- e.g., you can define "
		"an 'image' handler to which mime.type.image.png, "
		"mime.type.image.jpeg, and so on will refer; or one might "
		"define a handler for a more specific type of file -- e.g., "
		"PDF files. Note you must define both a MIME handler "
		"and a MIME type association for it to work.")),

	INIT_OPT_TREE("mime.handler", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Description of this handler.")),

	INIT_OPT_TREE("mime.handler._template_", NULL,
		"_template_", 0,
		N_("System-specific handler description "
		"(ie. unix, unix-xwin, ...).")),

	INIT_OPT_BOOL("mime.handler._template_._template_", N_("Ask before opening"),
		"ask", 0, 1,
		N_("Ask before opening.")),

	INIT_OPT_BOOL("mime.handler._template_._template_", N_("Block terminal"),
		"block", 0, 1,
		N_("Block the terminal when the handler is running.")),

	INIT_OPT_STRING("mime.handler._template_._template_", N_("Program"),
		"program", 0, "",
		/* xgettext:no-c-format */
		N_("External viewer for this file type. "
		"'%' in this string will be substituted by a file name. "
		"Do _not_ put single- or double-quotes around the % sign.")),


	INIT_OPT_TREE("mime", N_("File extension associations"),
		"extension", OPT_AUTOCREATE,
		N_("Extension <-> MIME type association.")),

	INIT_OPT_STRING("mime.extension", NULL,
		"_template_", 0, "",
		N_("MIME-type matching this file extension "
		"('*' is used here in place of '.').")),

#define INIT_OPT_MIME_EXTENSION(extension, type) \
	INIT_OPT_STRING("mime.extension", NULL, extension, 0, type, NULL)

	INIT_OPT_MIME_EXTENSION("gif",		"image/gif"),
	INIT_OPT_MIME_EXTENSION("jpg",		"image/jpg"),
	INIT_OPT_MIME_EXTENSION("jpeg",		"image/jpeg"),
	INIT_OPT_MIME_EXTENSION("png",		"image/png"),
	INIT_OPT_MIME_EXTENSION("txt",		"text/plain"),
	INIT_OPT_MIME_EXTENSION("htm",		"text/html"),
	INIT_OPT_MIME_EXTENSION("html",		"text/html"),
#ifdef CONFIG_BITTORRENT
	INIT_OPT_MIME_EXTENSION("torrent",	"application/x-bittorrent"),
#endif
#ifdef CONFIG_DOM
	INIT_OPT_MIME_EXTENSION("rss",		"application/rss+xml"),
	INIT_OPT_MIME_EXTENSION("xbel",		"application/xbel+xml"),
	INIT_OPT_MIME_EXTENSION("sgml",		"application/docbook+xml"),
#endif

	NULL_OPTION_INFO,
};

static unsigned char *
get_content_type_default(unsigned char *extension)
{
	struct option *opt_tree;
	struct option *opt;
	unsigned char *extend = extension + strlen(extension) - 1;

	if (extend < extension)	return NULL;

	opt_tree = get_opt_rec_real(config_options, "mime.extension");

	foreach (opt, *opt_tree->value.tree) {
		unsigned char *namepos = opt->name + strlen(opt->name) - 1;
		unsigned char *extpos = extend;

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
get_mime_type_option(unsigned char *type)
{
	struct option *opt;
	struct string name;

	opt = get_opt_rec_real(config_options, "mime.type");
	if (!opt) return NULL;

	if (!init_string(&name)) return NULL;

	if (add_optname_to_string(&name, type, strlen(type))) {
		/* Search for end of the base type. */
		unsigned char *pos = strchr(name.source, '/');

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
get_mime_handler_default(unsigned char *type, int have_x)
{
	struct option *type_opt = get_mime_type_option(type);
	struct option *handler_opt;

	if (!type_opt) return NULL;

	handler_opt = get_mime_handler_option(type_opt, have_x);
	if (!handler_opt) return NULL;

	return init_mime_handler(get_opt_str_tree(handler_opt, "program"),
				 type_opt->value.string,
				 default_mime_module.name,
				 get_opt_bool_tree(handler_opt, "ask"),
				 get_opt_bool_tree(handler_opt, "block"));
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
