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

def ELinks::follow_url_hook(url)
    return url
end


# Called when a HTML document has been loaded - before the document rendering
# begins. Makes it possible to fix up bad HTML code, remove tags etc.

def ELinks::pre_format_html_hook(url, html)
    return html
end
 

# Determining what proxy, if any, should be used to load a requested URL.
# The hook should return:
#
# * "PROXY:PORT" - to use the specified proxy
# * ""		 - to not use any proxy
# * nil		 - to use the default proxies

def ELinks::proxy_hook(url)
    return nil
end


# Called when ELinks quits and can be used to do required clean-ups

def ELinks::quit_hook
end
