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
