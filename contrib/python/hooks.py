dumbprefixes = {
	"7th" : "http://7thguard.net/",
	"b" : "http://babelfish.altavista.com/babelfish/tr",
	"bz" : "http://bugzilla.elinks.cz",
	"bug" : "http://bugzilla.elinks.cz",
	"d" : "http://www.dict.org",
	"g" : "http://www.google.com/",
	"gg" : "http://www.google.com/",
	"go" : "http://www.google.com/",
	"fm" : "http://www.freshmeat.net/",
	"sf" : "http://www.sourceforge.net/",
	"dbug" : "http://bugs.debian.org/",
	"dpkg" : "http://packages.debian.org/",
	"pycur" : "http://www.python.org/doc/current/",
	"pydev" : "http://www.python.org/dev/doc/devel/",
	"pyhelp" : "http://starship.python.net/crew/theller/pyhelp.cgi",
	"pyvault" : "http://www.vex.net/parnassus/",
	"e2" : "http://www.everything2.org/",
	"sd" : "http://www.slashdot.org/"
}

def goto_url_hook(url, current_url):
	global dumbprefixes

	if dumbprefixes.has_key(url):
		return dumbprefixes[url];
	else:
		return None

def follow_url_hook(url):
	return None

def pre_format_html_hook(url, html):
	return None

def proxy_for_hook(url):
	return None

def quit_hook():
	return None
