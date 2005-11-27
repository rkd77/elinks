# = hooks.rb - ELinks/Ruby hooks
#
# == Module Constants
#
# The following global module constants are defined
#
# * ELinks::VERSION	- The ELinks version. :-P
#
# * ELinks::HOME	- The path to ELinks configuration files
#
# == Debugging Scripts
#
# When debugging you can use either
#
#	p(obj, ...)
#
# or
#
#	message(string)
#
# to print message strings. The printed strings will be shown in a message
# box.


# Called when the user enters something into the goto URL dialog.
#
# Arguments:
# @url		the URL entered in the goto URL dialog.
# @current_url	the URL currently being viewed in ELinks.
#
# Returns the URL to go to. Return nil to use the @url argument as is
# (leave it untouched).

def ELinks::goto_url_hook(url, current_url)
    case url
    when /^localhost$/
	return "http://localhost/"

    when /test-ruby/
	# Dump the exported variables.
	message(ELinks::VERSION + " - " + ELinks::HOME);
	return current_url
    end

    return url
end


# Called when the user decides to load some document by following a link,
# entering an URL in the goto URL dialog, loading frames from a frameset (?)
# etc.
#
# Arguments:
# @url		the URL being followed.
#
# Returns the URL to be followed. Return nil to use the @url argument as is.

def ELinks::follow_url_hook(url)
    return url
end


# Called when a HTML document has been loaded - before the document rendering
# begins. Makes it possible to fix up bad HTML code, remove tags etc.
#
# Arguments:
# @url		the URL of the document being loaded.
# @html		the source of the document being loaded.
#
# Returns the preformatted source of the document. Return nil to leave the
# document source untouched.

def ELinks::pre_format_html_hook(url, html)
    if url.grep("fvwm.lair.be\/(index|viewforum)*.\.php")
	# I don't like the fact that the <img> tags provide labels as
	# well as span classes.  So we'll remove them.
        html.gsub!(/(<img src.*\"No new posts\".*\>)/,"<font color=\"green\">No New Posts</font></td>")
	html.gsub!(/(<img src.*\"New posts\".*\>)/,"<font color=\"red\">New Posts</font></td>")
	html.gsub!(/<td><span class=\"gensmall\">([nN]o)|[Nn]ew posts\<\/span\>\<\/td\>/,"<td></td>")
    end
    return html
end


# Determining what proxy, if any, should be used to load a requested URL.
#
# Arguments:
# @url		the URL to be proxied.
#
# The hook should return one of the following:
# 1. "<host>:<port>" - to use the specified host and port
# 2. ""		     - to not use any proxy
# 3. nil	     - to use the default proxies

def ELinks::proxy_hook(url)
    return nil
end


# Called when ELinks quits and can be used to do required clean-ups like
# removing any temporary files created by the hooks.

def ELinks::quit_hook
end
