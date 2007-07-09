/* Common code for smartprefixes_classic.js and smartprefixes_bookmarks.js. */

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

/* javascript:gitweb("http://repo.or.cz/gitweb.cgi", "elinks.git", "%s"); */
function gitweb(base, project, url)
{
	var parts = url.match(/^(search|summary|shortlog|log|blob|commit|commitdiff|history|tree|tag)(\s(.*))?/);
	var query = '?p=' + project;

	if (parts) {
		query += ';a=' + parts[1];

		/* If the extra arg is not for searching assume it is an ID. */
		if (parts[1] == 'search' && parts[3])
			query += ';s=' + escape(parts[3]);
		else if ((parts[1] == 'blob' || parts[1] == 'history' || parts[1] == 'tree') && parts[3])
			query += ';f=' + escape(parts[3]);
		else if (parts[3])
			query += ';h=' + escape(parts[3]);

	} else {
		query += ';a=summary';
	}

	return base + query;
}

/* javascript:gmane("%s") */
function gmane (url)
{
	var v = url.split(' ');
	var group = v[0], words = v.slice(1).join(' ');

	if (!group) return base_url;

	if (!words) {
		if (group.match(/^gmane\./)) {
			/* Looks like a newsgroup. */
			return "http://dir.gmane.org/" + group;
		} else {
			/* Looks like a mailing list. */
			return "http://gmane.org/find.php?list=" + group;
		}
	}

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

/* javascript:babelfish("%s"); */
function babelfish (url)
{
	var lang2code = {
		"chinese-simp": 'zh',
		"chinese-simple": 'zh',
		"chinese-simplified": 'zh',
		"chinese-trad": 'zt',
		"chinese-traditional": 'zt',
		"dutch": 'nl',
		"nederlands": 'nl',
		"Nederlands": 'nl',
		"german": 'de',
		"deutsch": 'de',
		"Deutsch": 'de',
		"english": 'en',
		"french": 'fr',
		"fran\231ais": 'fr',
		"greek": 'el',
		"italian": 'it',
		"italiano": 'it',
		"japanese": 'ja',
		"korean": 'ko',
		"portuguese": 'pt',
		"portugu\234s": 'pt',
		"russian": 'ru',
		"spanish": 'es',
		"espanol": 'es',
		"espa\241ol": 'es',
	};

	var parts = url.match(/^(\S+)\s+(\S+)\s*(.*)/);
	if (!parts) return "";

	var from = parts[1], to = parts[2], text = parts[3];

	if (lang2code[from]) from = lang2code[from];
	if (lang2code[to]) to = lang2code[to];

	if (text.match(/:[^[:blank:]]/))
		url = "http://babelfish.altavista.com/babelfish/urltrurl?url=";
	else
		url = "http://babelfish.altavista.com/babelfish/tr?trtext=";

	url += escape(text) + "&lp=" + from + "_" + to;

	return url;
}

/* javascript:videodownloader("%s"); */
function videodownloader (url)
{
	elinks.load_uri("http://videodownloader.net/get/?url=" + escape(url),
	                function (cached) {
	                        var url = cached.content.match(/<a href="(.*?)"><img src=".\/vd\/botdl.gif"/)[1]
	                        if (url) elinks.location = url;
	                } );

	return "";
}
