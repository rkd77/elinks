/* URI rewriting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "intl/libintl.h"
#include "main/event.h"
#include "main/module.h"
#include "protocol/rewrite/rewrite.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "util/string.h"


enum uri_rewrite_type {
	URI_REWRITE_DUMB,
	URI_REWRITE_SMART,
};


/* TODO: An event hook for follow-url might also feel at home here. --jonas */

enum uri_rewrite_option {
	URI_REWRITE_TREE,

	URI_REWRITE_ENABLE_DUMB,
	URI_REWRITE_ENABLE_SMART,

	URI_REWRITE_DUMB_TREE,
	URI_REWRITE_DUMB_TEMPLATE,

	URI_REWRITE_SMART_TREE,
	URI_REWRITE_SMART_TEMPLATE,

	URI_REWRITE_OPTIONS,
};

static union option_info uri_rewrite_options[] = {
	INIT_OPT_TREE(C_("protocol"), N_("URI rewriting"),
		C_("rewrite"), OPT_SORT,
		N_("Rules for rewriting URIs entered in the goto dialog. "
		"It makes it possible to define a set of prefixes that will "
		"be expanded if they match a string entered in the goto "
		"dialog. The prefixes can be dumb, meaning that they work "
		"only like URI abbreviations, or smart ones, making it "
		"possible to pass arguments to them like search engine "
		"keywords.")),

	INIT_OPT_BOOL(C_("protocol.rewrite"), N_("Enable dumb prefixes"),
		C_("enable-dumb"), OPT_ZERO, 1,
		N_("Enable dumb prefixes - simple URI abbreviations which "
		"can be written to the Goto URL dialog instead of actual URIs "
		"- i.e. if you write 'elinks' there, you are directed to "
		"http://elinks.cz/.")),

	INIT_OPT_BOOL(C_("protocol.rewrite"), N_("Enable smart prefixes"),
		C_("enable-smart"), OPT_ZERO, 1,
		N_("Enable smart prefixes - URI templates triggered by "
		"writing given abbreviation to the Goto URL dialog followed "
		"by a list of arguments from which the actual URI is composed "
		"- i.e. 'gg:search keywords' or 'gn search keywords for "
		"news'.")),

	INIT_OPT_TREE(C_("protocol.rewrite"), N_("Dumb Prefixes"),
		C_("dumb"), OPT_AUTOCREATE | OPT_SORT,
		N_("Dumb prefixes, see enable-dumb description for details.")),

	INIT_OPT_STRING(C_("protocol.rewrite.dumb"), NULL,
		C_("_template_"), OPT_ZERO, "",
		/* xgettext:no-c-format */
		N_("Replacement URI for this dumbprefix:\n"
		"%c in the string means the current URL\n"
		"%% in the string means '%'")),

	INIT_OPT_TREE(C_("protocol.rewrite"), N_("Smart Prefixes"),
		C_("smart"), OPT_AUTOCREATE | OPT_SORT,
		N_("Smart prefixes, see enable-smart description for "
		"details.")),

	/* TODO: In some rare occations current link URI and referrer might
	 * also be useful and dare I mention some kind of proxy argument. --jonas */
	INIT_OPT_STRING(C_("protocol.rewrite.smart"), NULL,
		C_("_template_"), OPT_ZERO, "",
		/* xgettext:no-c-format */
		N_("Replacement URI for this smartprefix:\n"
		"%c in the string means the current URL\n"
		"%s in the string means the whole argument to smartprefix\n"
		"%0,%1,...,%9 means argument 0, 1, ..., 9\n"
		"%% in the string means '%'")),

	INIT_OPT_STRING(C_("protocol.rewrite"), N_("Default template"),
		C_("default_template"), OPT_ZERO, "",
		/* xgettext:no-c-format */
		N_("Default URI template used when the string entered in "
		"the goto dialog does not appear to be a URI or a filename "
		"(i.e. contains no '.', ':' or '/' characters), and does "
		"not match any defined prefixes. Set the value to \"\" to "
		"disable use of the default template rewrite rule.\n"
		"\n"
		"%c in the template means the current URL,\n"
		"%s in the template means the whole string from the goto\n"
		"   dialog,\n"
		"%0,%1,...,%9 mean the 1st,2nd,...,10th space-delimited part\n"
		"   of %s,\n"
		"%% in the template means '%'.")),

#define INIT_OPT_DUMB_PREFIX(prefix, uri) \
	INIT_OPT_STRING(C_("protocol.rewrite.dumb"), NULL, prefix, OPT_ZERO, uri, NULL)

	INIT_OPT_DUMB_PREFIX(C_("elinks"), ELINKS_WEBSITE_URL),
	INIT_OPT_DUMB_PREFIX(C_("documentation"), ELINKS_DOC_URL),
	INIT_OPT_DUMB_PREFIX(C_("bz"), ELINKS_BUGS_URL),
	INIT_OPT_DUMB_PREFIX(C_("bug"), ELINKS_BUGS_URL),

	INIT_OPT_DUMB_PREFIX(C_("arc"), "https://web.archive.org/web/*/%c"),
	INIT_OPT_DUMB_PREFIX(C_("d"), "http://www.dict.org/bin/Dict"),
	INIT_OPT_DUMB_PREFIX(C_("ddg"), "https://lite.duckduckgo.com/lite"),
	INIT_OPT_DUMB_PREFIX(C_("g"), "https://www.google.com"),
	INIT_OPT_DUMB_PREFIX(C_("gg"), "https://www.google.com"),
	INIT_OPT_DUMB_PREFIX(C_("go"), "https://www.google.com"),
	INIT_OPT_DUMB_PREFIX(C_("fc"), "https://freshcode.club/"),
	INIT_OPT_DUMB_PREFIX(C_("sf"), "https://sourceforge.net"),
	INIT_OPT_DUMB_PREFIX(C_("dbug"), "https://www.debian.org/Bugs/"),
	INIT_OPT_DUMB_PREFIX(C_("dpkg"), "https://www.debian.org/distrib/packages"),
	INIT_OPT_DUMB_PREFIX(C_("lua"), "file:///usr/share/doc/lua/contents.html#index"),
	INIT_OPT_DUMB_PREFIX(C_("pycur"), "https://www.python.org/doc/"),
	INIT_OPT_DUMB_PREFIX(C_("pydev"), "https://docs.python.org/dev/"),
	INIT_OPT_DUMB_PREFIX(C_("e2"), "https://www.everything2.org"),
	INIT_OPT_DUMB_PREFIX(C_("sd"), "https://slashdot.org/"),
	INIT_OPT_DUMB_PREFIX(C_("vhtml"), "https://validator.w3.org/nu/?doc=%c"),
	INIT_OPT_DUMB_PREFIX(C_("vcss"), "https://jigsaw.w3.org/css-validator/validator?uri=%c"),

#define INIT_OPT_SMART_PREFIX(prefix, uri) \
	INIT_OPT_STRING(C_("protocol.rewrite.smart"), NULL, prefix, OPT_ZERO, uri, NULL)

	INIT_OPT_SMART_PREFIX(C_("arc"), "https://web.archive.org/web/*/%s"),
	INIT_OPT_SMART_PREFIX(C_("aur"), "https://aur.archlinux.org/packages/?K=%s"),
	INIT_OPT_SMART_PREFIX(C_("aw"), "https://wiki.archlinux.org/index.php?search=%s"),
	INIT_OPT_SMART_PREFIX(C_("bug"), ELINKS_BUGS_URL "?q=is:issue+%s"),
	INIT_OPT_SMART_PREFIX(C_("cambridge"), "https://dictionary.cambridge.org/dictionary/english/%s"),
	INIT_OPT_SMART_PREFIX(C_("cliki"), "http://www.cliki.net/site/search?words=%s"),
	INIT_OPT_SMART_PREFIX(C_("d"), "http://www.dict.org/bin/Dict?Query=%s&Form=Dict1&Strategy=*&Database=*&submit=Submit+query"),
	INIT_OPT_SMART_PREFIX(C_("ddg"), "https://duckduckgo.com/lite?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("foldoc"), "http://foldoc.org/?%s"),
	INIT_OPT_SMART_PREFIX(C_("g"), "https://www.google.com/search?q=%s"),
	/* Whose idea was it to use 'gg' for websearches? -- Miciah */
	/* INIT_OPT_SMART_PREFIX("gg", "https://groups.google.com/forum/#!search/%s"), */
	INIT_OPT_SMART_PREFIX(C_("gg"), "https://www.google.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("gi"), "https://www.google.com/search?q=%s&tbm=isch"),
	INIT_OPT_SMART_PREFIX(C_("gn"), "https://news.google.com/search?q=%s&hl=en-US&gl=US&ceid=US:en"),
	INIT_OPT_SMART_PREFIX(C_("go"), "https://www.google.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("gr"), "https://groups.google.com/forum/#!search/%s"),
	INIT_OPT_SMART_PREFIX(C_("google"), "https://www.google.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("gwho"), "http://www.googlism.com/search/?ism=%s&type=1"),
	INIT_OPT_SMART_PREFIX(C_("gwhat"), "http://www.googlism.com/search/?ism=%s&type=2"),
	INIT_OPT_SMART_PREFIX(C_("gwhere"), "http://www.googlism.com/search/?ism=%s&type=3"),
	INIT_OPT_SMART_PREFIX(C_("gwhen"), "http://www.googlism.com/search/?ism=%s&type=4"),
	INIT_OPT_SMART_PREFIX(C_("savannah"), "https://savannah.nongnu.org/search/?words=%s&type_of_search=soft&exact=1"),
	INIT_OPT_SMART_PREFIX(C_("sf"), "https://sourceforge.net/search/?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("sfp"), "https://sourceforge.net/projects/%s"),
	INIT_OPT_SMART_PREFIX(C_("dbug"), "https://bugs.debian.org/%s"),
	INIT_OPT_SMART_PREFIX(C_("dpkg"), "https://packages.debian.org/search?keywords=%s"),
	INIT_OPT_SMART_PREFIX(C_("emacs"), "https://www.emacswiki.org/emacs?match=%s&pages=on&permanentanchors=on"),
	INIT_OPT_SMART_PREFIX(C_("onelook"), "https://onelook.com/?w=%s"),
	INIT_OPT_SMART_PREFIX(C_("e2"), "https://www.everything2.org/?node=%s"),
	INIT_OPT_SMART_PREFIX(C_("encz"), "https://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=0&ecd_lines=15&ecd_hptxt=0"),
	INIT_OPT_SMART_PREFIX(C_("czen"), "https://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=1&ecd_lines=15&ecd_hptxt=0"),
	INIT_OPT_SMART_PREFIX(C_("dict"), "https://dictionary.reference.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("thes"), "https://www.thesaurus.com/browse/%s"),
	INIT_OPT_SMART_PREFIX(C_("a"), "https://www.acronymfinder.com/~/search/af.aspx?String=exact&Acronym=%s"),
	INIT_OPT_SMART_PREFIX(C_("imdb"), "https://www.imdb.com/find?q=%s"),
	INIT_OPT_SMART_PREFIX(C_("mw"), "https://www.merriam-webster.com/dictionary?s=%s"),
	INIT_OPT_SMART_PREFIX(C_("mwt"), "https://www.merriam-webster.com/thesaurus/%s"),
	INIT_OPT_SMART_PREFIX(C_("wiki"), "https://en.wikipedia.org/w/index.php?search=%s"),
	INIT_OPT_SMART_PREFIX(C_("wn"), "http://wordnetweb.princeton.edu/perl/webwn?s=%s"),
	/* Search the Free Software Directory */
	INIT_OPT_SMART_PREFIX(C_("fsd"), "https://directory.fsf.org/wiki?title=Special%3ASearch&search=%s"),
	/* rfc by number */
	INIT_OPT_SMART_PREFIX(C_("rfc"), "https://www.rfc-editor.org/rfc/rfc%s.txt"),
	/* rfc search */
	INIT_OPT_SMART_PREFIX(C_("rfcs"), "https://www.rfc-editor.org/search/rfc_search_detail.php?title=%s"),
	INIT_OPT_SMART_PREFIX(C_("cr"), "https://www.rfc-editor.org/search/rfc_search_detail.php?title=%s"),

	NULL_OPTION_INFO,
};

#define get_opt_rewrite(which)	uri_rewrite_options[(which)].option
#define get_dumb_enable()	get_opt_rewrite(URI_REWRITE_ENABLE_DUMB).value.number
#define get_smart_enable()	get_opt_rewrite(URI_REWRITE_ENABLE_SMART).value.number

static inline struct option *
get_prefix_tree(enum uri_rewrite_option tree)
{
	assert(tree == URI_REWRITE_DUMB_TREE
	       || tree == URI_REWRITE_SMART_TREE);
	return &get_opt_rewrite(tree);
}

#define MAX_URI_ARGS 10

static char *
rewrite_uri(char *url, struct uri *current_uri, const char *arg)
{
	struct string n = NULL_STRING;
	const char *args[MAX_URI_ARGS];
	int argslen[MAX_URI_ARGS];
	int argc = 0;
	int i;

	if (!init_string(&n)) return NULL;

	/* Extract space separated list of arguments */
	args[argc] = arg;
	for (i = 0; ; i++) {
		if (args[argc][i] == ' ') {
			argslen[argc] = i;
			argc++;
			if (argc == MAX_URI_ARGS) break;
			args[argc] = &args[argc - 1][i];
			i = 0;
			for (; *args[argc] == ' '; args[argc]++);
		} else if (!args[argc][i]) {
			argslen[argc] = i;
			argc++;
			break;
		}
	}

	while (*url) {
		int p;
		int value;

		for (p = 0; url[p] && url[p] != '%'; p++);

		add_bytes_to_string(&n, url, p);
		url += p;

		if (*url != '%') continue;

		url++;
		switch (*url) {
			case 'c':
				if (!current_uri) break;
				add_uri_to_string(&n, current_uri, URI_ORIGINAL);
				break;
			case 's':
				if (arg) encode_uri_string(&n, arg, -1, 1);
				break;
			case '%':
				add_char_to_string(&n, '%');
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				value = *url - '0';
				if (value >= argc) break;
				encode_uri_string(&n, args[value],
						  argslen[value], 1);
				break;
			default:
				add_bytes_to_string(&n, url - 1, 2);
				break;
		}
		if (*url) url++;
	}

	return n.source;
}

static char *
get_uri_rewrite_prefix(enum uri_rewrite_type type, char *url)
{
	enum uri_rewrite_option tree = type == URI_REWRITE_DUMB
			? URI_REWRITE_DUMB_TREE : URI_REWRITE_SMART_TREE;
	struct option *prefix_tree = get_prefix_tree(tree);
	struct option *opt = get_opt_rec_real(prefix_tree, url);
	char *exp = opt ? opt->value.string : NULL;

	return (exp && *exp) ? exp : NULL;
}

static enum evhook_status
goto_url_hook(va_list ap, void *data)
{
	char **url = va_arg(ap, char **);
	struct session *ses = va_arg(ap, struct session *);
	char *uu = NULL;
	const char *arg = "";
	char *argstart = *url + strcspn(*url, " :");

	if (get_smart_enable() && *argstart) {
		unsigned char bucket = *argstart;

		*argstart = '\0';
		uu = get_uri_rewrite_prefix(URI_REWRITE_SMART, *url);
		*argstart = bucket;
		arg = argstart + 1;
	}

	if (get_dumb_enable() && !uu && !*argstart)
		uu = get_uri_rewrite_prefix(URI_REWRITE_DUMB, *url);

	if (!uu
	    && !strchr(*url, ':')
	    && !strchr(*url, '.')
	    && !strchr(*url, '/')) {
		uu = get_opt_str("protocol.rewrite.default_template", NULL);
		if (uu && *uu) {
			arg = *url;
		} else {
			uu = NULL;
		}
	}

	if (uu) {
		struct uri *uri = ses && have_location(ses)
				? cur_loc(ses)->vs.uri : NULL;

		uu = rewrite_uri(uu, uri, arg);
		if (uu) {
			mem_free(*url);
			*url = uu;
		}
	}

	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info uri_rewrite_hooks[] = {
	{ "goto-url", -1, goto_url_hook, {NULL} },

	NULL_EVENT_HOOK_INFO
};

struct module uri_rewrite_module = struct_module(
	/* name: */		N_("URI rewrite"),
	/* options: */		uri_rewrite_options,
	/* hooks: */		uri_rewrite_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
