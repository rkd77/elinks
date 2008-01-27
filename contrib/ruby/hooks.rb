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

# -----
# LAST UPDATE:  2006/06/21 -- Thomas Adam <thomas.adam22@gmail.com>
# -----

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
	# Currently used for debugging the exported variables.
	message(ELinks::VERSION + " - " + ELinks::HOME);
	return current_url
    end

    return url
end


# Called when the user decides to load some document by following a link,
# entering a URL in the goto URL dialog, loading frames from a frameset (?)
# etc.
#
# Arguments:
# @url		the URL being followed.
#
# Returns the URL to be followed. Return nil to use the @url argument as is.

def ELinks::follow_url_hook(url)
    return url
end


# Called when an HTML document has been loaded - before the document rendering
# begins. Makes it possible to fix up bad HTML code, remove tags etc.
#
# Arguments:
# @url		the URL of the document being loaded.
# @html		the source of the document being loaded.
#
# Returns the preformatted source of the document. Return nil to leave the
# document source untouched.

def ELinks::pre_format_html_hook(url, html)
  # Things start here.  Note the match alternation to define those sites
  # which you want to rerender.

  # This regexp object is for matching against specific domains.
  domains="fvwm.lair.be|troubledlands.com|forums.gentoo.org"
  r = Regexp.new('http:\/\/'"#{domains}"'\/.*')
  s = Regexp.new('http:\/\/'"#{domains}"'\/view(forum|topic)|posting.*\.(html|php).*')
  
  if url.match(/http:\/\/edulinux.homeunix.org\/elinks-render\.html/)
    html.gsub!(/(<p class=\"example\">)(.*?)(<\/p>)/, '<h2><font color="red">\2</font></h2>')
    return html
  end

    if url.match(r)
        
        # Handle viewtopic.php -- this has some very slightly different
        # code to the other pages found on phpBB sites.  Again, the match
        # alternations are important here -- I ought to define a Regexp
        # object to handle this eventually, which would save code
        # duplication, at any rate.
        if url.match(s)

            # Start with the menubar at the top, and remove the image
            # links.  We still have to add the description back in so that
            # we know what the link references.  :)
            html.gsub!(/<(img.*?)>(faq|search|memberlist|usergroups|register|profile|you have no new messages|log out \[.*?\]|chat room<\/a>)/i,'\2')

            # Remove the long list of relative links (I don't like them).
            html.gsub!(/<link rel.*? \/>/,'')

            # Turn on borders for the "main" table.
            html.gsub!(/(<table class=\"forumline\".*border=)(\".*?\")(.*?>)/,'\11\3')
            
            # In the LHS of the table for each postee, remove their image
            # link, and just display their status instead.
            html.gsub!(/(<span class=\"postdetails\">.*?)(<img src.*? \/>)/i,'\1')

            # Handle the options for each post.  Depending on the status
            # of the person viewing this site, they could have none or all
            # of these -- either way they've been shortened.
            html.gsub!(/(<img src.*?)(alt=)(\"Reply With Quote\")\s*(title=\"reply with quote\")(.*? \/>)/i, '\1\2Q \4')
            html.gsub!(/(<img src.*?)(alt=)(\"edit\/delete this post\")\s*(title=\"edit\/delete this post\")(.*? \/>)/i, '\1\2E/D \4')
            html.gsub!(/(<img src.*?)(alt=)(\"delete this post\")\s*(title=\"delete this post\")(.*? \/>)/i, '\1\2D \4')
            html.gsub!(/(<img src.*?)(alt=)(\"post\")\s*(title=\"post\")(.*? \/>)/i, '\1\2--&gt; \4')
	    html.gsub!(/(<img src.*?)(alt=)(\"new post\")\s*(title=\"new post\")(.*? \/>)/i, '\1\2--&gt; \4')
	    html.gsub!(/(<img src.*?)(alt=)(\"view newest post\")\s*(title=\"view newest post\")(.*? \/>)/i, '\1\2--&gt; \4')
	    html.gsub!(/(<img src.*?)(alt=)(\"view latest post\")\s*(title=\"view latest post\")(.*? \/>)/i, '\1\2&lt;-- \4')


            # See above.
            html.gsub!(/(<img src.*?)(alt=)(\"view ip address of poster\")\s*(title=\"view ip address of poster\")(.*? \/>)/i, '\1\2IP />')
	    html.gsub!(/<table cellspacing=\"0\" cellpadding=\"0\"\s*border=\"0\"\s*height="18"\s*width=\"18\">/, '<table border="0">')
            html.gsub!(/(<img src.*?)(alt=)(\"view user\'s profile\")\s*(title=\"view user\'s profile\")(.*? \/>)/i, '\1\2Profile  />')
            html.gsub!(/(<img src.*?)(alt=)(\"send private message\")\s*(title=\"send private message\")(.*? \/>)/i, '\1\2PM />')
            html.gsub!(/(<img src.*?)(alt=)(\"send e-mail\")\s*(title=\"send e-mail\")(.*? \/>)/i, '\1\2email />')
            html.gsub!(/(<img src.*?)(alt=)(\"visit poster\'s website\")\s*(title=\"visit poster\'s website\")(.*? \/>)/i, '\1\2Web />&nbsp;')
            html.gsub!(/(<img src.*?)(alt=)(\"Yahoo Messenger\")\s*(title=\"Yahoo Messenger\")(.*? \/>)/i, '\1\2Yahoo />')
            html.gsub!(/(<img src.*?)(alt=)(\"Msn Messenger\")\s*(title=\"Msn Messenger\")(.*? \/>)/i, '\1\2MSN />') 
        end
        
        # PhpBB's index page defines its layout as a series of tables.
        # The main table has a class of 'forumline' -- hence setting its
        # border property to 1 helps readability and to proide distinct
        # lines between the table sections.
        html.gsub!(/(<table.*border=)(\".*?\")\s*class=\"forumline\"\s*(.*?>)/,'\11\3')
        
        # Change the background colour of the forum pages to white -- the
        # default is grey.
        html.gsub!(/(body \{\n\tbackground-color: )(\#E5E5E5\;)/,'\1white;')
        
        # Assuming you've told ELinks to remember your login details (not
        # that it matters), the "New posts" and "No New Post" indicators
        # were typically just defined in words.   This looked ugly.  Hence
        # in this case, I've changed the table structure to change the
        # background colour of these cells.  A red colour indicates "No new
        # posts", whereas a green colour indicates "New Posts".
        html.gsub!(/(<td class=\"row1\".*?)>(<img src.*No new posts.*>)/, '\1 bgcolor="red">&nbsp;</td>')
        html.gsub!(/(<td class=\"row1\".*?)>(<img src.*new posts.*>)/i, '\1 bgcolor="green">&nbsp;</td>')
        
        # These lines appear at the bottom of the pages as a kind of "key"
        # for the above changes.
        html.gsub!(/(<img src.*\"No new posts\".*\>)/,"<font color=\"red\">No New Posts</font></td>")
        html.gsub!(/(<img src.*\"New posts\".*\>)/,"<font color=\"green\">New Posts</font></td>")
        
        # Remove duplicate lines within the HTML that would otherwise
        # repeat the same thing.  (Damn annoying)
        html.gsub!(/<td><span class=\"gensmall\">([nN]o)|[Nn]ew posts\<\/span\>\<\/td\>/,"<td></td>")
        
        # As with the red/green background colour for (No|New) Posts
        # above, a blue colour in the table indicates that the selected
        # thread is locked.
        html.gsub!(/(<td class=\"row1\".*?)>(<img src.*locked.*>)/i, '\1 bgcolor="blue">&nbsp;</td>')
        
        # Orange indicates that the selected thread has moved.
        html.gsub!(/(<td class=\"row1\".*?)>(<img src.*moved.*>)/i, '\1 bgcolor="orange">&nbsp;</td>')
        
        # Remove image repetitions at the bottom of the page entirely (the
        # only ever serve as a key for the page above -- this is why I am
        # using colour here.)
        html.gsub!(/(<td class=\"gensmall\">(.*?<\/td>|<img src.*?><\/td>))/,'')
        html.gsub!(/(<td.*?>)(<img src.*alt=\"(Sticky|Announcement|.*Popular.*)\".* \/><\/td>)/i, '')
        html.gsub!(/(<td.*?>)(<img src.*alt=\"Forum is locked\" \/><\/td>)/,'')
    
        # At the very top of the pages, remove the images, but retain the
        # links by way of their alt attributes.  (For things like FAQ,
        # MemeberList, etc.)
	#
	# DON'T do this for the pages matched by regexp 's' -- to do so
	# would remove many other images those pages rely on (and that
	# have been munged in the 's' section above, anyway.)
        html.gsub!(/<(img.*?)>(.*?<\/a>)/i, '\2') if not url.match(s)

	# OK -- "View Latest Post" becomes "[<--]" which hopefully points
	# to the person's name.
	html.gsub!(/(<img src.*?)(alt=)(\"view (newest|latest) post\")\s*(title=\"view (newest|latest) post\")(.*? \/>)/i, '\1\2&lt;-- \4') if not url.match(s)
	
	# And in a similar fashion, remove the "Goto Page" links.
	html.gsub!(/(<img src.*?alt=\"goto page\".*? \/>)/i,'')
        
        # No need to have this in the table at the bottom.
        html.gsub!(/(<td class=\"row1\".*?><img src.*alt=\"who is online\".* \/><\/td>)/i,'')

        # Remove all the Link: rel references
        html.gsub!(/<link rel.*? \/>/,'')

        # Some phpBB sites use frames.  Ugh
        html.gsub!(/(<table width=\"100%\" cellpadding=\"4\" cellspacing=\"1\" border=)1>/,'\10>')
        html.gsub!(/(<frameset.*?)(border=)\"2\"(.*)(frameborder=)\"(.*?)\"(.*>)/i,'\1\2"0"\3\4"no"\6')
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
