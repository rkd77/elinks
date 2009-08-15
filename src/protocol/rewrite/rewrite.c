/* URI rewriting module */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
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
	INIT_OPT_TREE("protocol", N_("URI rewriting"),
		"rewrite", OPT_SORT,
		N_("Rules for rewriting URIs entered in the goto dialog. "
		"It makes it possible to define a set of prefixes that will "
		"be expanded if they match a string entered in the goto "
		"dialog. The prefixes can be dumb, meaning that they work "
		"only like URI abbreviations, or smart ones, making it "
		"possible to pass arguments to them like search engine "
		"keywords.")),

	INIT_OPT_BOOL("protocol.rewrite", N_("Enable dumb prefixes"),
		"enable-dumb", 0, 1,
		N_("Enable dumb prefixes - simple URI abbreviations which "
		"can be written to the Goto URL dialog instead of actual URIs "
		"- i.e. if you write 'elinks' there, you are directed to "
		"http://elinks.cz/.")),

	INIT_OPT_BOOL("protocol.rewrite", N_("Enable smart prefixes"),
		"enable-smart", 0, 1,
		N_("Enable smart prefixes - URI templates triggered by "
		"writing given abbreviation to the Goto URL dialog followed "
		"by a list of arguments from which the actual URI is composed "
		"- i.e. 'gg:search keywords' or 'gn search keywords for "
		"news'.")),

	INIT_OPT_TREE("protocol.rewrite", N_("Dumb Prefixes"),
		"dumb", OPT_AUTOCREATE | OPT_SORT,
		N_("Dumb prefixes, see enable-dumb description for details.")),

	INIT_OPT_STRING("protocol.rewrite.dumb", NULL,
		"_template_", 0, "",
		/* xgettext:no-c-format */
		N_("Replacement URI for this dumbprefix:\n"
		"%c in the string means the current URL\n"
		"%% in the string means '%'")),

	INIT_OPT_TREE("protocol.rewrite", N_("Smart Prefixes"),
		"smart", OPT_AUTOCREATE | OPT_SORT,
		N_("Smart prefixes, see enable-smart description for "
		"details.")),

	/* TODO: In some rare occations current link URI and referrer might
	 * also be useful and dare I mention some kind of proxy argument. --jonas */
	INIT_OPT_STRING("protocol.rewrite.smart", NULL,
		"_template_", 0, "",
		/* xgettext:no-c-format */
		N_("Replacement URI for this smartprefix:\n"
		"%c in the string means the current URL\n"
		"%s in the string means the whole argument to smartprefix\n"
		"%0,%1,...,%9 means argument 0, 1, ..., 9\n"
		"%% in the string means '%'")),

	INIT_OPT_STRING("protocol.rewrite", N_("Default template"),
		"default_template", 0, "",
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
	INIT_OPT_STRING("protocol.rewrite.dumb", NULL, prefix, 0, uri, NULL)

	INIT_OPT_DUMB_PREFIX("elinks", ELINKS_WEBSITE_URL),
	INIT_OPT_DUMB_PREFIX("documentation", ELINKS_DOC_URL),
	INIT_OPT_DUMB_PREFIX("bz", ELINKS_BUGS_URL),
	INIT_OPT_DUMB_PREFIX("bug", ELINKS_BUGS_URL),

	INIT_OPT_DUMB_PREFIX("arc", "http://web.archive.org/web/*/%c"),
	INIT_OPT_DUMB_PREFIX("cia", "http://cia.navi.cx/"),
	INIT_OPT_DUMB_PREFIX("b", "http://babelfish.altavista.com/babelfish/tr"),
	INIT_OPT_DUMB_PREFIX("d", "http://www.dict.org"),
	INIT_OPT_DUMB_PREFIX("g", "http://www.google.com/"),
	INIT_OPT_DUMB_PREFIX("gg", "http://www.google.com/"),
	INIT_OPT_DUMB_PREFIX("go", "http://www.google.com/"),
	INIT_OPT_DUMB_PREFIX("fm", "http://freshmeat.net/"),
	INIT_OPT_DUMB_PREFIX("sf", "http://www.sourceforge.net/"),
	INIT_OPT_DUMB_PREFIX("dbug", "http://bugs.debian.org/"),
	INIT_OPT_DUMB_PREFIX("dpkg", "http://packages.debian.org/"),
	/* Hm, is this Debian-centric? -- Miciah */
	/* Well, does anyone but Debian use lua40 naming convention? --pasky */
	INIT_OPT_DUMB_PREFIX("lua", "file:///usr/share/doc/lua40-doc/manual/idx.html"),
	INIT_OPT_DUMB_PREFIX("pycur", "http://www.python.org/doc/current/"),
	INIT_OPT_DUMB_PREFIX("pydev", "http://www.python.org/dev/doc/devel/"),
	INIT_OPT_DUMB_PREFIX("pyhelp", "http://starship.python.net/crew/theller/pyhelp.cgi"),
	INIT_OPT_DUMB_PREFIX("pyvault", "http://www.vex.net/parnassus/"),
	INIT_OPT_DUMB_PREFIX("e2", "http://www.everything2.org/"),
	INIT_OPT_DUMB_PREFIX("sd", "http://slashdot.org/"),
	INIT_OPT_DUMB_PREFIX("vhtml", "http://validator.w3.org/check?uri=%c"),
	INIT_OPT_DUMB_PREFIX("vcss", "http://jigsaw.w3.org/css-validator/validator?uri=%c"),

#define INIT_OPT_SMART_PREFIX(prefix, uri) \
	INIT_OPT_STRING("protocol.rewrite.smart", NULL, prefix, 0, uri, NULL)
#define bugzilla_prefix(prefix) (ELINKS_BUGS_URL prefix)

	INIT_OPT_SMART_PREFIX("bug", bugzilla_prefix("show_bug.cgi?id=%s")),

#ifdef CONFIG_DEBUG
	INIT_OPT_SMART_PREFIX("milestone-bugs", bugzilla_prefix("buglist.cgi?target_milestone=%s")),
	INIT_OPT_SMART_PREFIX("search-bugs", bugzilla_prefix("buglist.cgi?short_desc_type=allwordssubstr&short_desc=%s")),
#endif

	INIT_OPT_SMART_PREFIX("arc", "http://web.archive.org/web/*/%s"),
	INIT_OPT_SMART_PREFIX("bb", "http://babelfish.altavista.com/babelfish/tr?urltext=%s"),
	INIT_OPT_SMART_PREFIX("bb_fr_en", "http://babelfish.altavista.com/babelfish/tr?lp=fr_en&submit=1&urltext=%s"),
	INIT_OPT_SMART_PREFIX("bb_en_fr", "http://babelfish.altavista.com/babelfish/tr?lp=en_fr&submit=1&urltext=%s"),
	INIT_OPT_SMART_PREFIX("cambridge", "http://dictionary.cambridge.org/results.asp?searchword=%s"),
	INIT_OPT_SMART_PREFIX("cliki", "http://www.cliki.net/admin/search?words=%s"),
	INIT_OPT_SMART_PREFIX("d", "http://www.dict.org/bin/Dict?Query=%s&Form=Dict1&Strategy=*&Database=*&submit=Submit+query"),
	INIT_OPT_SMART_PREFIX("dmoz", "http://search.dmoz.org/cgi-bin/search?search=%s"),
	INIT_OPT_SMART_PREFIX("foldoc", "http://wombat.doc.ic.ac.uk/foldoc/foldoc.cgi?%s"),
	INIT_OPT_SMART_PREFIX("g", "http://www.google.com/search?q=%s&btnG=Google+Search"),
	INIT_OPT_SMART_PREFIX("gd", "http://www.google.com/search?q=%s&cat=gwd/Top"),
	/* Whose idea was it to use 'gg' for websearches? -- Miciah */
	/* INIT_OPT_SMART_PREFIX("gg", "http://groups.google.com/groups?q=%s"), */
	INIT_OPT_SMART_PREFIX("gg", "http://www.google.com/search?q=%s&btnG=Google+Search"),
	INIT_OPT_SMART_PREFIX("gi", "http://images.google.com/images?q=%s"),
	INIT_OPT_SMART_PREFIX("gn", "http://news.google.com/news?q=%s"),
	INIT_OPT_SMART_PREFIX("go", "http://www.google.com/search?q=%s&btnG=Google+Search"),
	INIT_OPT_SMART_PREFIX("gr", "http://groups.google.com/groups?q=%s"),
	INIT_OPT_SMART_PREFIX("google", "http://www.google.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX("gwho", "http://www.googlism.com/?ism=%s&name=1"),
	INIT_OPT_SMART_PREFIX("gwhat", "http://www.googlism.com/?ism=%s&name=2"),
	INIT_OPT_SMART_PREFIX("gwhere", "http://www.googlism.com/?ism=%s&name=3"),
	INIT_OPT_SMART_PREFIX("gwhen", "http://www.googlism.com/?ism=%s&name=4"),
	INIT_OPT_SMART_PREFIX("fm", "http://freshmeat.net/search/?q=%s"),
	INIT_OPT_SMART_PREFIX("savannah", "http://savannah.nongnu.org/search/?words=%s&type_of_search=soft&exact=1"),
	INIT_OPT_SMART_PREFIX("sf", "http://sourceforge.net/search/?q=%s"),
	INIT_OPT_SMART_PREFIX("sfp", "http://sourceforge.net/projects/%s"),
	INIT_OPT_SMART_PREFIX("sd", "http://slashdot.org/search.pl?query=%s"),
	INIT_OPT_SMART_PREFIX("sdc", "http://slashdot.org/search.pl?query=%s&op=comments"),
	INIT_OPT_SMART_PREFIX("sdu", "http://slashdot.org/search.pl?query=%s&op=users"),
	INIT_OPT_SMART_PREFIX("sdp", "http://slashdot.org/search.pl?query=%s&op=polls"),
	INIT_OPT_SMART_PREFIX("sdj", "http://slashdot.org/search.pl?query=%s&op=journals"),
	INIT_OPT_SMART_PREFIX("dbug", "http://bugs.debian.org/%s"),
	INIT_OPT_SMART_PREFIX("dpkg", "http://packages.debian.org/%s"),
	INIT_OPT_SMART_PREFIX("emacs", "http://www.emacswiki.org/cgi-bin/wiki.pl?search=%s"),
	INIT_OPT_SMART_PREFIX("lyrics", "http://music.lycos.com/lyrics/results.asp?QT=L&QW=%s"),
	INIT_OPT_SMART_PREFIX("lxr", "http://lxr.linux.no/ident?i=%s"),
	INIT_OPT_SMART_PREFIX("onelook", "http://onelook.com/?w=%s&ls=a"),
	INIT_OPT_SMART_PREFIX("py", "http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=current"),
	INIT_OPT_SMART_PREFIX("pydev", "http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=devel"),
	INIT_OPT_SMART_PREFIX("pyvault", "http://py.vaults.ca/apyllo.py?find=%s"),
	INIT_OPT_SMART_PREFIX("e2", "http://www.everything2.org/?node=%s"),
	INIT_OPT_SMART_PREFIX("encz", "http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=0&ecd_lines=15&ecd_hptxt=0"),
	INIT_OPT_SMART_PREFIX("czen", "http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=1&ecd_lines=15&ecd_hptxt=0"),
	INIT_OPT_SMART_PREFIX("dict", "http://dictionary.reference.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX("thes", "http://thesaurus.reference.com/search?q=%s"),
	INIT_OPT_SMART_PREFIX("a", "http://acronymfinder.com/af-query.asp?String=exact&Acronym=%s"),
	INIT_OPT_SMART_PREFIX("imdb", "http://imdb.com/Find?%s"),
	INIT_OPT_SMART_PREFIX("mw", "http://www.m-w.com/cgi-bin/dictionary?book=Dictionary&va=%s"),
	INIT_OPT_SMART_PREFIX("mwt", "http://www.m-w.com/cgi-bin/thesaurus?book=Thesaurus&va=%s"),
	INIT_OPT_SMART_PREFIX("whatis", "http://uptime.netcraft.com/up/graph/?host=%s"),
	INIT_OPT_SMART_PREFIX("wiki", "http://en.wikipedia.org/w/wiki.phtml?search=%s"),
	INIT_OPT_SMART_PREFIX("wn", "http://www.cogsci.princeton.edu/cgi-bin/webwn1.7.1?stage=1&word=%s"),
	/* Search the Free Software Directory */
	INIT_OPT_SMART_PREFIX("fsd", "http://directory.fsf.org/search/fsd-search.py?q=%s"),
	/* rfc by number */
	INIT_OPT_SMART_PREFIX("rfc", "http://www.rfc-editor.org/rfc/rfc%s.txt"),
	/* rfc search */
	INIT_OPT_SMART_PREFIX("rfcs", "http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25"),
	INIT_OPT_SMART_PREFIX("cr", "http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25"),
	/* Internet Draft search */
	INIT_OPT_SMART_PREFIX("rfcid", "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25"),
	INIT_OPT_SMART_PREFIX("id", "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25"),
	INIT_OPT_SMART_PREFIX("draft", "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25"),

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

static unsigned char *
rewrite_uri(unsigned char *url, struct uri *current_uri, unsigned char *arg)
{
	struct string n = NULL_STRING;
	unsigned char *args[MAX_URI_ARGS];
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

static unsigned char *
get_uri_rewrite_prefix(enum uri_rewrite_type type, unsigned char *url)
{
	enum uri_rewrite_option tree = type == URI_REWRITE_DUMB
			? URI_REWRITE_DUMB_TREE : URI_REWRITE_SMART_TREE;
	struct option *prefix_tree = get_prefix_tree(tree);
	struct option *opt = get_opt_rec_real(prefix_tree, url);
	unsigned char *exp = opt ? opt->value.string : NULL;

	return (exp && *exp) ? exp : NULL;
}

static enum evhook_status
goto_url_hook(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *uu = NULL;
	unsigned char *arg = "";
	unsigned char *argstart = *url + strcspn(*url, " :");

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
		uu = get_opt_str("protocol.rewrite.default_template");
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
	{ "goto-url", -1, goto_url_hook },

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
