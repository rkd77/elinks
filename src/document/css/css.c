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

	NULL_OPTION_INFO,
};


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
	unsigned char *url = get_opt_str("document.css.stylesheet");

	if (!list_empty(default_stylesheet.selectors))
		done_css_stylesheet(&default_stylesheet);

	if (!*url) return;

	import_css_file(&default_stylesheet, NULL, url, strlen(url));
}

static int
change_hook_css(struct session *ses, struct option *current, struct option *changed)
{
	if (!strcmp(changed->name, "stylesheet")) {
		/** @todo TODO: We need to update all entries in
		 * format cache. --jonas */
		import_default_css();
	}

	/* Instead of using the value of the @ses parameter, iterate
	 * through the @sessions list.  The parameter may be NULL and
	 * anyway we don't support session-specific options yet.  */
	foreach (ses, sessions) draw_formatted(ses, 1);

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
