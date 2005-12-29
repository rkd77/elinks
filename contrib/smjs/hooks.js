/* These are examples for the ELinks SpiderMonkey scripting interface.
 * Place choice parts in a file named "hooks.js" in your ELinks configuration
 * directory (~/.elinks).
 */

elinks.keymaps.main["@"] = function () {
	elinks.location = elinks.location + "/..";
};

elinks.preformat_html_hooks = new Array();
elinks.preformat_html = function (cached) {
	for (var i in elinks.preformat_html_hooks)
		if (!elinks.preformat_html_hooks[i](cached))
			return false;

	return true;
};

elinks.goto_url_hooks = new Array();
elinks.goto_url_hook = function (url) {
	for (var i in elinks.goto_url_hooks){ 
		url = elinks.goto_url_hooks[i](url);
		if (typeof(url) == 'boolean' && !url) return false;
	}

	return url;
};

elinks.follow_url_hooks = new Array();
elinks.follow_url_hook = function (url) {
	for (var i in elinks.follow_url_hooks) {
		url = elinks.follow_url_hooks[i](url);
		if (typeof(url) == 'boolean' && !url) return false;
	}

	return url;
};

function root_w00t(cached) {
	cached.content = cached.content.replace(/root/g, "w00t");
	return true;
};
elinks.preformat_html_hooks.push(root_w00t);

function mangle_deb_bugnumbers(cached) {
	if (!cached.uri.match(/^[a-z0-9]+:\/\/[a-z0-9A-Z.-]+debian\.org/)
	    && !cached.uri.match(/changelog\.Debian/))
		return true;

	var num_re = /([0-9]+)/g;
	var rewrite_closes_fn = function (str) {
		return str.replace(num_re,
		                 '<a href="http://bugs.debian.org/$1">$1</a>');
	}
	/* Debian Policy Manual 4.4 footnote 16 */
	var closes_re = /closes:\s*(?:bug)?\#?\s?\d+(?:,\s*(?:bug)?\#?\s?\d+)*/gi;

	cached.content = cached.content.replace(closes_re, rewrite_closes_fn);

	return true;
}
elinks.preformat_html_hooks.push(mangle_deb_bugnumbers);

function rewrite_uri(uri) {
	if (!elinks.bookmarks.smartprefixes) return true;

	var parts = uri.split(" ");
	var prefix = parts[0];

	if (!elinks.bookmarks.smartprefixes.children[prefix]) return true;

	var rule = elinks.bookmarks.smartprefixes.children[prefix].url;
	var rest = parts.slice(1).join(" ");

	if (rule.match(/^javascript:/))
		return eval(rule
		             .replace(/^javascript:/, "")
		             .replace(/%s/, rest));

	return rule.replace(/%s/, escape(rest));
}
elinks.goto_url_hooks.push(rewrite_uri);

function block_pr0n(uri) {
	if (uri.match(/pr0n/)) {
		elinks.alert('No pr0n!');
		return "";
	}

	return true;
}
elinks.follow_url_hooks.push(block_pr0n);


/* The following functions are for use as smartprefixes. Create a top-level
 * folder titled "smartprefixes". In it, add a bookmark for each smartprefix,
 * putting the keyword in the title and either a normal URI or some JavaScript
 * code prefixed with "javascript:" as the URI. When you enter the keyword
 * in the Go to URL box, ELinks will take the URI of the corresponding bookmark,
 * replace any occurrence of "%s" with the rest of the text entered in the Go to
 * URL box, evaluate the code if the URI starts with "javascript:", and go to
 * the resulting URI.
 */

/* Helper function for debian_contents and debian_file. */
function debian_package (url, t)
{
	url = url.replace(/(\w+):(\w+)/g,
	                  function (all, key, val) { t[key] = val; return ""; })

	return 'http://packages.debian.org/cgi-bin/search_contents.pl?word='
	 + escape(url.replace(/\s*(\S+)\s*/, '$1'))
	 + '&searchmode=' + (t.searchmode || 'searchfilesanddirs')
	 + '&case=' + (t["case"] || 'insensitive')
	 + '&version=' + (t.version || 'stable')
	 + '&arch=' + (t.arch || 'i386')
}

/* javascript:debian_contents("%s"); */
function debian_contents (url)
{
	return debian_package (url, { searchmode: "filelist" })
}

/* javascript:debian_file("%s"); */
function debian_file (url)
{
	return debian_package (url, { searchmode: "searchfilesanddirs" })
}

/* javascript:cvsweb("http://cvsweb.elinks.cz/cvsweb.cgi/", "elinks", "%s"); */
function cvsweb (base, project, url)
{
	/* <file>:<revision>[-><revision>] */
	url = url.replace(/^(.*):(.*?)(?:->(.*))?$/, "$1 $2 $3");

	var parts = url.split(" ");
	if (parts[3]) {
		elinks.alert('this smartprefix takes only one to three arguments');
		return "";
	}

	var file = parts[0], oldrev = parts[1], newrev = parts[2];
	if (!file) {
		elinks.alert('no file given');
		return "";
	}

	if (newrev)
		return base + project + "/" + file + ".diff"
		        + "?r1=" + oldrev + "&r2=" + newrev + "&f=u";

	if (oldrev)
		return base + "~checkout~/" + project + "/" + file
		        + (oldrev != "latest" && "?rev=" + oldrev || "");

	return base + project + "/" + file
}

/* javascript:gmane("%s") */
function gmane (url)
{
	var v = url.split(' ');
	var group = v[0], words = v.slice(1).join(' ');

	if (!words) return "";

	return "http://search.gmane.org/search.php?query=" + words
	        + "&group=" + group;
}

/* javascript:bugzilla('http://bugzilla.elinks.cz/', "%s"); */
function bugzilla (base_url, arguments)
{
	if (!arguments || arguments == '') return base_url;

	if (arguments.match(/^[\d]+$/))
		return base_url + 'show_bug.cgi?id=' + arguments;

	return base_url + 'buglist.cgi?short_desc_type=allwordssubstr'
                + '&short_desc=' + escape(arguments);
}
