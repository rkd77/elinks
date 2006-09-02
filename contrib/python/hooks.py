import re
import sys

dumbprefixes = {
	"7th" : "http://7thguard.net/",
	"b" : "http://babelfish.altavista.com/babelfish/tr/",
	"bz" : "http://bugzilla.elinks.cz/",
	"bug" : "http://bugzilla.elinks.cz/",
	"d" : "http://www.dict.org/",
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

cygwin = re.compile("cygwin\.com")
cygwin_sub1 = re.compile('<body bgcolor="#000000" color="#000000"')
cygwin_sub2 = '<body bgcolor="#ffffff" color="#000000"'
mbank = re.compile('https://www\.mbank\.com\.pl/ib_navibar_3\.asp')
mbank_sub1 = re.compile('<td valign="top"><img')
mbank_sub2 = '<tr><td valign="top"><img'
google_redirect = re.compile('^http://www\.google\.com/url\?sa=D&q=(.*)')

def goto_url_hook(url, current_url):
	global dumbprefixes

	if dumbprefixes.has_key(url):
		return dumbprefixes[url];
	else:
		return None

def follow_url_hook(url):
	m = google_redirect.search(url)
	if m:
		return m.group(1)
	return None

def pre_format_html_hook(url, html):
	if cygwin.search(url):
		html2 = cygwin_sub1.sub(cygwin_sub2, html)
		return html2
	if mbank.search(url):
		html2 = mbank_sub1.sub(mbank_sub2, html)
		return html2
	return None

def proxy_for_hook(url):
	return None

def quit_hook():
	return None
