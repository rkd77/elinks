"""Example Python hooks for ELinks.

If ELinks is compiled with an embedded Python interpreter, it will try
to import a Python module called hooks when the browser starts up. To
use Python code from within ELinks, create a file called hooks.py in
the ~/.elinks directory, or in the system-wide configuration directory
(defined when ELinks was compiled), or in the standard Python search path.
An example hooks.py file can be found in the contrib/python directory of
the ELinks source distribution.

The hooks module may implement any of several functions that will be
called automatically by ELinks in appropriate circumstances; it may also
bind ELinks keystrokes to callable Python objects so that arbitrary Python
code can be invoked at the whim of the user.

Functions that will be automatically called by ELinks (if they're defined):

follow_url_hook() -- Rewrite a URL for a link that's about to be followed.
goto_url_hook() -- Rewrite a URL received from a "Go to URL" dialog box.
pre_format_html_hook() -- Rewrite a document's body before it's formatted.
proxy_for_hook() -- Determine what proxy server to use for a given URL.
quit_hook() -- Clean up before ELinks exits.

"""

import elinks

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

def goto_url_hook(url):
    """Rewrite a URL that was entered in a "Go to URL" dialog box.

    This function should return a string containing a URL for ELinks to
    follow, or an empty string if no URL should be followed, or None if
    ELinks should follow the original URL.

    Arguments:

    url -- The URL provided by the user.

    """
    if url in dumbprefixes:
        return dumbprefixes[url]

def follow_url_hook(url):
    """Rewrite a URL for a link that's about to be followed.

    This function should return a string containing a URL for ELinks to
    follow, or an empty string if no URL should be followed, or None if
    ELinks should follow the original URL.

    Arguments:

    url -- The URL of the link.

    """
    google_redirect = 'http://www.google.com/url?sa=D&q='
    if url.startswith(google_redirect):
        return url.replace(google_redirect, '')

def pre_format_html_hook(url, html):
    """Rewrite the body of a document before it's formatted.

    This function should return a string for ELinks to format, or None
    if ELinks should format the original document body. It can be used
    to repair bad HTML, alter the layout or colors of a document, etc.

    Arguments:

    url -- The URL of the document.
    html -- The body of the document.

    """
    if "cygwin.com" in url:
        return html.replace('<body bgcolor="#000000" color="#000000"',
                            '<body bgcolor="#ffffff" color="#000000"')
    elif url.startswith("https://www.mbank.com.pl/ib_navibar_3.asp"):
        return html.replace('<td valign="top"><img',
                            '<tr><td valign="top"><img')

def proxy_for_hook(url):
    """Determine what proxy server to use for a given URL.

    This function should return a string of the form "hostname:portnumber"
    identifying a proxy server to use, or an empty string if the request
    shouldn't use any proxy server, or None if ELinks should use its
    default proxy server.

    Arguments:

    url -- The URL that is about to be followed.

    """
    pass

def quit_hook():
    """Clean up before ELinks exits.

    This function should handle any clean-up tasks that need to be
    performed before ELinks exits. Its return value is ignored.

    """
    pass


# The rest of this file demonstrates some of the functionality available
# within hooks.py through the embedded Python interpreter's elinks module.
# Full documentation for the elinks module (as well as the hooks module)
# can be found in doc/python.txt in the ELinks source distribution.


# This class shows how to use elinks.input_box() and elinks.open(). It
# creates a dialog box to prompt the user for a URL, and provides a callback
# function that opens the URL in a new tab.
#
class goto_url_in_new_tab:
    """Prompter that opens a given URL in a new tab."""
    def __init__(self):
        """Prompt for a URL."""
        elinks.input_box("Enter URL", self._callback, title="Go to URL")
    def _callback(self, url):
        """Open the given URL in a new tab."""
        if 'goto_url_hook' in globals():
            # Mimic the standard "Go to URL" dialog by calling goto_url_hook().
            url = goto_url_hook(url) or url
        if url:
            elinks.open(url, new_tab=True)
# The elinks.bind_key() function can be used to create a keystroke binding
# that will instantiate the class.
#
elinks.bind_key("Ctrl-g", goto_url_in_new_tab)


# This class can be used to enter arbitrary Python expressions for the embedded
# interpreter to evaluate. (Note that it can only evalute Python expressions,
# not execute arbitrary Python code.) The callback function will use
# elinks.info_box() to display the results.
#
class simple_console:
    """Simple console for passing expressions to the Python interpreter."""
    def __init__(self):
        """Prompt for a Python expression."""
        elinks.input_box("Enter Python expression", self._callback, "Console")
    def _callback(self, input):
        """Evalute input and display the result (unless it's None)."""
        if input is None:
            # The user canceled.
            return
        result = eval(input)
        if result is not None:
            elinks.info_box(result, "Result")
elinks.bind_key("F5", simple_console)


# If you edit ~/.elinks/hooks.py while the browser is running, you can use
# this function to make your changes take effect immediately (without having
# to restart ELinks).
#
def reload_hooks():
    """Reload the ELinks Python hooks."""
    import hooks
    reload(hooks)
elinks.bind_key("F6", reload_hooks)


# This example demonstrates how to create a menu by providing a sequence of
# (string, function) tuples specifying each menu item.
#
def menu():
    """Let the user choose from a menu of Python functions."""
    items = (
        ("~Go to URL in new tab", goto_url_in_new_tab),
        ("Simple Python ~console", simple_console),
        ("~Reload Python hooks", reload_hooks),
        ("Read my favorite RSS/ATOM ~feeds", feedreader),
    )
    elinks.menu(items)
elinks.bind_key("F4", menu)


# Finally, a more elaborate demonstration: If you install the add-on Python
# module from http://feedparser.org/ this class can use it to parse a list
# of your favorite RSS/ATOM feeds, figure out which entries you haven't seen
# yet, open each of those entries in a new tab, and report statistics on what
# it fetched. The class is instantiated by pressing the "!" key.
#
# This class demonstrates the elinks.load() function, which can be used to
# load a document into the ELinks cache without displaying it to the user;
# the document's contents are passed to a Python callback function.

my_favorite_feeds = (
        "http://rss.gmane.org/messages/excerpts/gmane.comp.web.elinks.user",
        # ... add any other RSS or ATOM feeds that interest you ...
        # "http://elinks.cz/news.rss",
        # "http://www.python.org/channews.rdf",
)

class feedreader:

    """RSS/ATOM feed reader."""

    def __init__(self, feeds=my_favorite_feeds):
        """Constructor."""
        if elinks.home is None:
            raise elinks.error("Cannot identify unread entries without "
                               "a ~/.elinks configuration directory.")
        self._results = {}
        self._feeds = feeds
        for feed in feeds:
            elinks.load(feed, self._callback)

    def _callback(self, header, body):
        """Read a feed, identify unseen entries, and open them in new tabs."""
        import anydbm
        import feedparser # you need to get this module from feedparser.org
        import os

        if not body:
            return
        seen = anydbm.open(os.path.join(elinks.home, "rss.seen"), "c")
        feed = feedparser.parse(body)
        new = 0
        errors = 0
        for entry in feed.entries:
            try:
                if not seen.has_key(entry.link):
                    elinks.open(entry.link, new_tab=True)
                    seen[entry.link] = ""
                    new += 1
            except:
                errors += 1
        seen.close()
        self._tally(feed.channel.title, new, errors)

    def _tally(self, title, new, errors):
        """Record and report feed statistics."""
        self._results[title] = (new, errors)
        if len(self._results) == len(self._feeds):
            feeds = self._results.keys()
            feeds.sort()
            width = max([len(title) for title in feeds])
            fmt = "%*s   new entries: %2d   errors: %2d\n"
            summary = ""
            for title in feeds:
                new, errors = self._results[title]
                summary += fmt % (-width, title, new, errors)
            elinks.info_box(summary, "Feed Statistics")

elinks.bind_key("!", feedreader)
