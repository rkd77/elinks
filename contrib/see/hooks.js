/* Hooks for the ELinks SEE browser scripting
 *
 * Copyright (c) Jonas Fonseca, 2005
 */

function quit() {
	// ELinks.write("quiting ... " + ELinks.version);
}

/*********************************************************************
 *  goto_url(url, current_url)
 *********************************************************************/

var goto_url_hooks = []

function goto_url(url, current_url)
{
	var context = {
		url: url,
		current_url: current_url ? current_url : ""
	}

	for (var i = 0; i < goto_url_hooks.length; i++)
		if (goto_url_hooks[i](context, current_url))
			break

	return context.url
}

// Don't take localhost as directory name
function expand_localhost(context)
{
	if (context.url.match(/localhost/)) {
		context.url = "http://" + context.url + "/"
		return true
	}

	return false
}

goto_url_hooks.push(expand_localhost)

// You can write smt like "gg" to goto URL dialog and it'll go to google.com.
// Note that this is obsoleted by the URI rewrite plugin.

var dumbprefixes = {
	arc:		"http://web.archive.org/web/*/%c",
	b:		"http://babelfish.altavista.com/babelfish/tr",
	bz:		"http://bugzilla.elinks.or.cz",
	bug:		"http://bugzilla.elinks.or.cz",
	d:		"http://www.dict.org",
	g:		"http://www.google.com/",
	gg:		"http://www.google.com/",
	go:		"http://www.google.com/",
	fm:		"http://www.freshmeat.net/",
	sf:		"http://www.sourceforge.net/",
	dbug:		"http://bugs.debian.org/",
	dpkg:		"http://packages.debian.org/",
	pycur:		"http://www.python.org/doc/current/",
	pydev:		"http://www.python.org/dev/doc/devel/",
	pyhelp:		"http://starship.python.net/crew/theller/pyhelp.cgi",
	pyvault:	"http://www.vex.net/parnassus/",
	e2:		"http://www.everything2.org/",
	sd:		"http://www.slashdot.org/",
	vhtml:		"http://validator.w3.org/check?uri=%c",
	vcss:		"http://jigsaw.w3.org/css-validator/validator?uri=%c"
}

function expand_dumbprefix(context, current_url)
{
	if (dumbprefixes[context.url]) {
		context.url = dumbprefixes[context.url].replace(/%c/, context.current_url)
		return true
	}

	return false
}

goto_url_hooks.push(expand_dumbprefix)


var smartprefixes = {
	arc:		"http://web.archive.org/web/*/%s",
	cambridge:	"http://dictionary.cambridge.org/results.asp?searchword=%s",
	cliki:		"http://www.cliki.net/admin/search?words:	%s",
	d:		"http://www.dict.org/bin/Dict?Query:	%s&Form=Dict1&Strategy=*&Database=*&submit=Submit+query",
	dmoz:		"http://search.dmoz.org/cgi-bin/search?search=%s",
	foldoc:		"http://wombat.doc.ic.ac.uk/foldoc/foldoc.cgi?%s",
	g:		"http://www.google.com/search?q=%s&btnG=Google+Search",
	gd:		"http://www.google.com/search?q=%s&cat=gwd/Top",
	gg:		"http://www.google.com/search?q=%s&btnG=Google+Search",
	// Whose idea was it to use 'gg' for websearches? -- Miciah
	//gg = "http://groups.google.com/groups?q=%s",
	gi:		"http://images.google.com/images?q=%s",
	gn:		"http://news.google.com/news?q=%s",
	go:		"http://www.google.com/search?q=%s&btnG=Google+Search",
	gwho:		"http://www.googlism.com/?ism=%s&name=1",
	gwhat:		"http://www.googlism.com/?ism=%s&name=2",
	gwhere:		"http://www.googlism.com/?ism=%s&name=3",
	gwhen:		"http://www.googlism.com/?ism=%s&name=4",
	fm:		"http://www.freshmeat.net/search/?q=%s",
	savannah:	"http://savannah.nongnu.org/search/?words=%s&type_of_search=soft&exact=1",
	sf:		"http://sourceforge.net/search/?q=%s",
	sfp:		"http://sourceforge.net/projects/%s",
	sd:		"http://www.slashdot.org/search.pl?query=%s",
	sdc:		"http://www.slashdot.org/search.pl?query=%s&op=comments",
	sdu:		"http://www.slashdot.org/search.pl?query=%s&op=users",
	sdp:		"http://www.slashdot.org/search.pl?query=%s&op=polls",
	sdj:		"http://www.slashdot.org/search.pl?query=%s&op=journals",
	dbug:		"http://bugs.debian.org/%s",
	dpkg:		"http://packages.debian.org/%s",
	emacs:		"http://www.emacswiki.org/cgi-bin/wiki.pl?search=%s",
	lyrics:		"http://music.lycos.com/lyrics/results.asp?QT=L&QW=%s",
	lxr:		"http://lxr.linux.no/ident?i=%s",
	leo:		"http://dict.leo.org/?search=%s",
	onelook:	"http://onelook.com/?w=%s&ls=a",
	py:		"http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=current",
	pydev:		"http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=devel",
	pyvault:	"http://py.vaults.ca/apyllo.py?find=%s",
	e2:		"http://www.everything2.org/?node=%s",
	encz:		"http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=0&ecd_lines=15&ecd_hptxt=0",
	czen:		"http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=1&ecd_lines=15&ecd_hptxt=0",
	dict:		"http://dictionary.reference.com/search?q=%s",
	thes:		"http://thesaurus.reference.com/search?q=%s",
	a:		"http://acronymfinder.com/af-query.asp?String=exact&Acronym=%s",
	imdb:		"http://imdb.com/Find?%s",
	mw:		"http://www.m-w.com/cgi-bin/dictionary?book=Dictionary&va=%s",
	mwt:		"http://www.m-w.com/cgi-bin/thesaurus?book=Thesaurus&va=%s",
	whatis:		"http://uptime.netcraft.com/up/graph/?host=%s",
	wiki:		"http://www.wikipedia.org/w/wiki.phtml?search=%s",
	wn:		"http://www.cogsci.princeton.edu/cgi-bin/webwn1.7.1?stage=1&word=%s",
	// rfc by number
	rfc:		"http://www.rfc-editor.org/rfc/rfc%s.txt",
	// rfc search
	rfcs:		"http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
	cr:		"http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
	// Internet Draft search
	rfcid:		"http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
	urbandict:	"http://www.urbandictionary.com/define.php?term=%s",
	id:		"http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
	draft:		"http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25"
}


function expand_smartprefix(context, current_url)
{
	var match = context.url.match(/^([^:\s]+)(:|\s)\s*(.*)\s*$/)

	if (match && match[1] && match[3]) {
		var nick = match[1]
		var val = match[3]
		
		if (smartprefixes[nick]) {
			if (typeof smartprefixes[nick] == 'string') {
				context.url = smartprefixes[nick].replace(/%s/, escape(val))
				return true

			} else if (typeof smartprefixes[nick] == 'object'
			    && smartprefixes[nick].constructor == Function) {
				context.url = smartprefixes[nick](val)
				return true

			} else {
				ELinks.write('smartprefix "' + nick + '" has unsupported type "' + typeof smartprefixes[nick] + '".')
				return false
			}
		}
	}

	// Unmatched.
	return false
}

goto_url_hooks.push(expand_smartprefix)
