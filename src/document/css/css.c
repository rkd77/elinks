/** CSS module management
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/home.h"
#include "config/options.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/stylesheet.h"
#include "encoding/encoding.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/draw.h"


union option_info css_options_info[] = {
	INIT_OPT_TREE("document", N_("Cascading Style Sheets"),
		"css", OPT_SORT,
		N_("Options concerning how to use CSS for styling "
		"documents.")),

	INIT_OPT_BOOL("document.css", N_("Enable CSS"),
		"enable", 0, 1,
		N_("Enable adding of CSS style info to documents.")),

	INIT_OPT_BOOL("document.css", N_("Ignore \"display: none\""),
		"ignore_display_none", 0, 1,
		N_("When enabled, elements are rendered, even when their "
		"display property has the value \"none\". Because ELinks's "
		"CSS support is still very incomplete, this setting can "
		"improve the way that some documents are rendered.")),

	INIT_OPT_BOOL("document.css", N_("Import external style sheets"),
		"import", 0, 1,
		N_("When enabled any external style sheets that are imported "
		"from either CSS itself using the @import keyword or from the "
		"HTML using <link> tags in the document header will also be "
		"downloaded.")),

	INIT_OPT_STRING("document.css", N_("Default style sheet"),
		"stylesheet", 0, "",
		N_("The path to the file containing the default user defined "
		"Cascading Style Sheet. It can be used to control the basic "
		"layout of HTML documents. The path is assumed to be relative "
		"to ELinks' home directory.\n"
		"\n"
		"Leave as \"\" to use built-in document styling.")),

	INIT_OPT_STRING("document.css", N_("Media types"),
		"media", 0, "tty",
		N_("CSS media types that ELinks claims to support, separated "
		"with commas. The \"all\" type is implied. Currently, only "
		"ASCII characters work reliably here.  See CSS2 section 7: "
		"http://www.w3.org/TR/1998/REC-CSS2-19980512/media.html")),

	NULL_OPTION_INFO,
};


/** Check whether ELinks claims to support a specific CSS media type.
 *
 * @param optstr
 *   Null-terminated value of the document.css.media option.
 * @param token
 *   A name parsed from a CSS file or from an HTML media attribute.
 *   Need not be null-terminated.
 * @param token_length
 *   Length of @a token, in bytes.
 *
 * Both strings should be in the ASCII charset.
 *
 * @return nonzero if the media type is supported, 0 if not.  */
int
supports_css_media_type(const unsigned char *optstr,
			const unsigned char *token, size_t token_length)
{
	/* Split @optstr into comma-delimited strings, strip leading
	 * and trailing spaces from each, and compare them to the
	 * token.  */
	while (*optstr != '\0') {
		const unsigned char *beg, *end;

		while (*optstr == ' ')
			++optstr;

		beg = optstr;
		optstr += strcspn(optstr, ",");
		end = optstr;
		while (end > beg && end[-1] == ' ')
			--end;

		/* W3C REC-html401-19991224 section 6.13:
		 *   "3. A case-sensitive match is then made with
		 *    the set of media types defined above."
		 * W3C REC-html401-19991224 section 14.2.3:
		 *   "media = media-descriptors [CI]"
		 *   where CI stands for case-insensitive.
		 * This mismatch has been reported to the
		 * www-html-editor@w3.org mailing list on 2000-11-16.
		 *
		 * W3C REC-CSS2-19980512 section 7.3:
		 *  "Media type names are case-insensitive."  */
		if (!strlcasecmp(token, token_length, beg, end - beg))
			return 1;

		while (*optstr == ',')
			++optstr;
	}

	/* An explicit "all" is probably rarer than e.g. "tty".  */
	if (!strlcasecmp(token, token_length, "all", 3))
		return 1;

	return 0;
}

void
import_css(struct css_stylesheet *css, struct uri *uri)
{
	/* Do we have it in the cache? (TODO: CSS cache) */
	struct cache_entry *cached;
	struct fragment *fragment;

	if (!uri || css->import_level >= MAX_REDIRECTS)
		return;

	cached = get_redirected_cache_entry(uri);
	if (!cached) return;

	fragment = get_cache_fragment(cached);
	if (fragment) {
		unsigned char *end = fragment->data + fragment->length;

		css->import_level++;
		css_parse_stylesheet(css, uri, fragment->data, end);
		css->import_level--;
	}
}


static void
import_css_file(struct css_stylesheet *css, struct uri *base_uri,
		const unsigned char *url, int urllen)
{
	struct string string, filename;

	if (!*url
	    || css->import_level >= MAX_REDIRECTS
	    || !init_string(&filename))
		return;

	if (*url != '/' && elinks_home) {
		add_to_string(&filename, elinks_home);
	}

	add_bytes_to_string(&filename, url, urllen);

	if (is_in_state(read_encoded_file(&filename, &string), S_OK)) {
		unsigned char *end = string.source + string.length;

		css->import_level++;
		css_parse_stylesheet(css, base_uri, string.source, end);
		done_string(&string);
		css->import_level--;
	}

	done_string(&filename);
}

struct css_stylesheet default_stylesheet = INIT_CSS_STYLESHEET(default_stylesheet, import_css_file);

static void
import_default_css(void)
{
	unsigned char *url = get_opt_str("document.css.stylesheet", NULL);

	if (!css_selector_set_empty(&default_stylesheet.selectors))
		done_css_stylesheet(&default_stylesheet);

	if (!*url) return;

	import_css_file(&default_stylesheet, NULL, url, strlen(url));
}

static int
change_hook_css(struct session *ses, struct option *current, struct option *changed)
{
	int reload_css = 0;

	if (!strcmp(changed->name, "stylesheet")) {
		/** @todo TODO: We need to update all entries in
		 * format cache. --jonas */
		import_default_css();
	}

	if (!strcmp(changed->name, "media"))
		reload_css = 1;

	/* Instead of using the value of the @ses parameter, iterate
	 * through the @sessions list.  The parameter may be NULL and
	 * anyway we don't support session-specific options yet.  */
	foreach (ses, sessions) draw_formatted(ses, 1 + reload_css);

	return 0;
}

static void
init_css(struct module *module)
{
	static const struct change_hook_info css_change_hooks[] = {
		{ "document.css",		change_hook_css },
		{ NULL,				NULL },
	};

	register_change_hooks(css_change_hooks);
	import_default_css();
}

void
done_css(struct module *module)
{
	done_css_stylesheet(&default_stylesheet);
}


struct module css_module = struct_module(
	/* name: */		N_("Cascading Style Sheets"),
	/* options: */		css_options_info,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_css,
	/* done: */		done_css
);
