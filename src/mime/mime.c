/* Functionality for handling mime types */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "encoding/encoding.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "mime/backend/common.h"
#include "mime/mime.h"
#include "protocol/header.h"	/* For parse_header() */
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


enum mime_options {
	MIME_TREE,
	MIME_DEFAULT_TYPE,

	MIME_OPTIONS,
};

static union option_info mime_options[] = {
	INIT_OPT_TREE("", N_("MIME"),
		"mime", OPT_SORT,
		N_("MIME-related options (handlers of various MIME types).")),

	INIT_OPT_STRING("mime", N_("Default MIME-type"),
		"default_type", 0, DEFAULT_MIME_TYPE,
		N_("Document MIME-type to assume by default "
		"(when we are unable to guess it properly "
		"from known information about the document).")),

	NULL_OPTION_INFO,
};

#define get_opt_mime(which)	mime_options[(which)].option
#define get_default_mime_type()	get_opt_mime(MIME_DEFAULT_TYPE).value.string

/* Checks protocols headers for a suitable filename */
static unsigned char *
get_content_filename(struct uri *uri, struct cache_entry *cached)
{
	unsigned char *filename, *pos;

	if (!cached) cached = find_in_cache(uri);

	if (!cached || !cached->head)
		return NULL;

	pos = parse_header(cached->head, "Content-Disposition", NULL);
	if (!pos) return NULL;

	parse_header_param(pos, "filename", &filename);
	mem_free(pos);
	if (!filename) return NULL;

	/* Remove start and ending quotes. */
	if (filename[0] == '"') {
		int len = strlen(filename);

		if (len > 1 && filename[len - 1] == '"') {
			filename[len - 1] = 0;
			memmove(filename, filename + 1, len);
		}

		/* It was an empty quotation: "" */
		if (!filename[1]) {
			mem_free(filename);
			return NULL;
		}
	}

	/* We don't want to add any directories from the path so make sure we
	 * only add the filename. */
	pos = get_filename_position(filename);
	if (!*pos) {
		mem_free(filename);
		return NULL;
	}

	if (pos > filename)
		memmove(filename, pos, strlen(pos) + 1);

	return filename;
}

/* Checks if application/x-<extension> has any handlers. */
static inline unsigned char *
check_extension_type(unsigned char *extension)
{
	/* Trim the extension so only last .<extension> is used. */
	unsigned char *trimmed = strrchr(extension, '.');
	struct mime_handler *handler;
	unsigned char *content_type;

	if (!trimmed)
		return NULL;

	content_type = straconcat("application/x-", trimmed + 1,
				  (unsigned char *) NULL);
	if (!content_type)
		return NULL;

	handler = get_mime_type_handler(content_type, 1);
	if (handler) {
		mem_free(handler);
		return content_type;
	}

	mem_free(content_type);
	return NULL;
}

/* Check if part of the extension coresponds to a supported encoding and if it
 * has any handlers. */
static inline unsigned char *
check_encoding_type(unsigned char *extension)
{
	enum stream_encoding encoding = guess_encoding(extension);
	const unsigned char *const *extension_list;
	unsigned char *last_extension = strrchr(extension, '.');

	if (encoding == ENCODING_NONE || !last_extension)
		return NULL;

	for (extension_list = listext_encoded(encoding);
	     extension_list && *extension_list;
	     extension_list++) {
		unsigned char *content_type;

		if (strcmp(*extension_list, last_extension))
			continue;

		*last_extension = '\0';
		content_type = get_content_type_backends(extension);
		*last_extension = '.';

		return content_type;
	}

	return NULL;
}

#if 0
#define DEBUG_CONTENT_TYPE
#endif

#ifdef DEBUG_CONTENT_TYPE
#define debug_get_content_type_params(cached) \
	DBG("get_content_type(head, url)\n=== head ===\n%s\n=== url ===\n%s\n", (cached)->head, struri((cached)->uri))
#define debug_ctype(ctype__) DBG("ctype= %s", (ctype__))
#define debug_extension(extension__) DBG("extension= %s", (extension__))
#else
#define debug_get_content_type_params(cached)
#define debug_ctype(ctype__)
#define debug_extension(extension__)
#endif

unsigned char *
get_extension_content_type(unsigned char *extension)
{
	unsigned char *ctype;

	assert(extension && *extension);

	ctype = get_content_type_backends(extension);
	debug_ctype(ctype);
	if (ctype) return ctype;

	ctype = check_encoding_type(extension);
	debug_ctype(ctype);
	if (ctype) return ctype;

	ctype = check_extension_type(extension);
	debug_ctype(ctype);
	return ctype;
}

unsigned char *
get_cache_header_content_type(struct cache_entry *cached)
{
	unsigned char *extension, *ctype;

	ctype = parse_header(cached->head, "Content-Type", NULL);
	if (ctype) {
		unsigned char *end = strchr(ctype, ';');
		int ctypelen;

		if (end) *end = '\0';

		ctypelen = strlen(ctype);
		while (ctypelen && ctype[--ctypelen] <= ' ')
			ctype[ctypelen] = '\0';

		debug_ctype(ctype);

		if (*ctype) {
			return ctype;
		}

		mem_free(ctype);
	}

	/* This searches cached->head for filename so put here */
	extension = get_content_filename(cached->uri, cached);
	debug_extension(extension);
	if (extension) {
		ctype = get_extension_content_type(extension);
		mem_free(extension);
		if (ctype) {
			return ctype;
		}
	}

	return NULL;
}

static unsigned char *
get_fragment_content_type(struct cache_entry *cached)
{
	struct fragment *fragment;
	size_t length;
	unsigned char *sample;
	unsigned char *ctype = NULL;

	if (list_empty(cached->frag))
		return NULL;

	fragment = cached->frag.next;
	if (fragment->offset)
		return NULL;

	length = fragment->length > 1024 ? 1024 : fragment->length;
	sample = memacpy(fragment->data, length);
	if (!sample)
		return NULL;

	if (c_strcasestr(sample, "<html>"))
		ctype = stracpy("text/html");

	mem_free(sample);

	return ctype;
}

unsigned char *
get_content_type(struct cache_entry *cached)
{
	unsigned char *extension, *ctype;

	debug_get_content_type_params(cached);

	if (cached->content_type)
		return cached->content_type;

	/* If there's one in header, it's simple.. */
	if (cached->head) {
		ctype = get_cache_header_content_type(cached);
		if (ctype && *ctype) {
			cached->content_type = ctype;
			return ctype;
		}
		mem_free_if(ctype);
	}

	/* We can't use the extension string we are getting below, because we
	 * want to support also things like "ps.gz" - that'd never work, as we
	 * would always compare only to "gz". */
	/* Guess type accordingly to the extension */
	extension = get_extension_from_uri(cached->uri);
	debug_extension(extension);

	if (extension) {
		/* XXX:	A little hack for making extension handling case
		 * insensitive. We could probably do it better by making
		 * guess_encoding() case independent the real problem however
		 * is with default (via option system) and mimetypes resolving
		 * doing that option and hash lookup will not be easy to
		 * convert. --jonas */
		convert_to_lowercase_locale_indep(extension, strlen(extension));

		ctype = get_extension_content_type(extension);
		mem_free(extension);
		if (ctype && *ctype) {
			cached->content_type = ctype;
			return ctype;
		}
		mem_free_if(ctype);
	}

	ctype = get_fragment_content_type(cached);
	if (ctype && *ctype) {
		cached->content_type = ctype;
		return ctype;
	}

	debug_ctype(get_default_mime_type());

	/* Fallback.. use some hardwired default */
	cached->content_type = stracpy(get_default_mime_type());

	return cached->content_type;
}

struct mime_handler *
get_mime_type_handler(unsigned char *content_type, int xwin)
{
	return get_mime_handler_backends(content_type, xwin);
}

struct string *
add_mime_filename_to_string(struct string *string, struct uri *uri)
{
	unsigned char *filename = get_content_filename(uri, NULL);

	assert(uri->data);

	if (filename) {
		add_shell_safe_to_string(string, filename, strlen(filename));
		mem_free(filename);

		return string;
	}

	return add_uri_to_string(string, uri, URI_FILENAME);
}

/* Backends dynamic area: */

#include "mime/backend/default.h"
#include "mime/backend/mailcap.h"
#include "mime/backend/mimetypes.h"

static struct module *mime_submodules[] = {
	&default_mime_module,
#ifdef CONFIG_MAILCAP
	&mailcap_mime_module,
#endif
#ifdef CONFIG_MIMETYPES
	&mimetypes_mime_module,
#endif
	NULL,
};

struct module mime_module = struct_module(
	/* name: */		N_("MIME"),
	/* options: */		mime_options,
	/* hooks: */		NULL,
	/* submodules: */	mime_submodules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
