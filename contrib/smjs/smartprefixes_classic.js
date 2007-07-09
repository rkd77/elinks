/* Classic, table-based smartprefixes. */
 
var loaded_smartprefixes_common_code;
if (!loaded_smartprefixes_common_code) {
	do_file(elinks.home + "smartprefixes_common.js");
	loaded_smartprefixes_common_code = 1;
}

var smartprefixes = {
    arc:         "http://web.archive.org/web/*/%s",
    binsearch:   "http://binsearch.info/?q=%s",
    bug:         function (url) { return bugzilla('http://bugzilla.elinks.cz/', url) },
    cambridge:   "http://dictionary.cambridge.org/results.asp?searchword=%s",
    cliki:       "http://www.cliki.net/admin/search?words=%s",
    // If you want to add a smartprefix for another project's CVSweb,
    // just create a lambda like this. Aren't high-level languages fun?
    cvs:         function (x) { return cvsweb ("http://cvsweb.elinks.cz/cvsweb.cgi/", "elinks", x) },
    gitweb:      function (x) { return gitweb("http://repo.or.cz/gitweb.cgi", "elinks.git", x) },
    d:           "http://www.dict.org/bin/Dict?Query=%s&Form=Dict1&Strategy=*&Database=*&submit=Submit+query",
    debcontents: debian_contents,
    debfile:     debian_file,
    dix:         "http://dix.osola.com/?search=%s",
    dmoz:        "http://search.dmoz.org/cgi-bin/search?search=%s",
    foldoc:      "http://wombat.doc.ic.ac.uk/foldoc/foldoc.cgi?%s",
    g:           "http://www.google.com/search?q=%s&btnG=Google+Search",
    gd:          "http://www.google.com/search?q=%s&cat=gwd/Top",
    // Whose idea was it to use 'gg' for websearches? -- Miciah
    //gg:          "http://groups.google.com/groups?q=%s",
    gi:          "http://images.google.com/images?q=%s",
    gmane:       gmane,
    gn:          "http://news.google.com/news?q=%s",
    go:          "http://www.google.com/search?q=%s&btnG=Google+Search",
    gwho:        "http://www.googlism.com/?ism=%s&name=1",
    gwhat:       "http://www.googlism.com/?ism=%s&name=2",
    gwhere:      "http://www.googlism.com/?ism=%s&name=3",
    gwhen:       "http://www.googlism.com/?ism=%s&name=4",
    fm:          "http://www.freshmeat.net/search/?q=%s",
    savannah:    "http://savannah.nongnu.org/search/?words=%s&type_of_search=soft&exact=1",
    sf:          "http://sourceforge.net/search/?q=%s",
    sfp:         "http://sourceforge.net/projects/%s",
    sd:          "http://www.slashdot.org/search.pl?query=%s",
    sdc:         "http://www.slashdot.org/search.pl?query=%s&op=comments",
    sdu:         "http://www.slashdot.org/search.pl?query=%s&op=users",
    sdp:         "http://www.slashdot.org/search.pl?query=%s&op=polls",
    sdj:         "http://www.slashdot.org/search.pl?query=%s&op=journals",
    dbug:        "http://bugs.debian.org/%s",
    dix:         "http://dix.osola.com/index.de.php?trans=1&search=%s",
    dixgram:     "http://dix.osola.com/v.php?language=german&search=%s",
    dpkg:        "http://packages.debian.org/%s",
    emacs:       "http://www.emacswiki.org/cgi-bin/wiki.pl?search=%s",
    lyrics:      "http://music.lycos.com/lyrics/results.asp?QT=L&QW=%s",
    lxr:         "http://lxr.linux.no/ident?i=%s",
    leo:         "http://dict.leo.org/?search=%s",
    nclaw:       "http://www.ncleg.net/gascripts/Statutes/StatutesSearch.asp?searchScope=All&searchCriteria=%s&returnType=Section",
    onelook:     "http://onelook.com/?w=%s&ls=a",
    py:          "http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=current",
    pydev:       "http://starship.python.net/crew/theller/pyhelp.cgi?keyword=%s&version=devel",
    pyvault:     "http://py.vaults.ca/apyllo.py?find=%s",
    e2:          "http://www.everything2.org/?node=%s",
    encz:        "http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=0&ecd_lines=15&ecd_hptxt=0",
    czen:        "http://www.slovnik.cz/bin/ecd?ecd_il=1&ecd_vcb=%s&ecd_trn=translate&ecd_trn_dir=1&ecd_lines=15&ecd_hptxt=0",
    dict:        "http://dictionary.reference.com/search?q=%s",
    thes:        "http://thesaurus.reference.com/search?q=%s",
    a:           "http://acronymfinder.com/af-query.asp?String=exact&Acronym=%s",
    imdb:        "http://imdb.com/Find?%s",
    mw:          "http://www.m-w.com/cgi-bin/dictionary?book=Dictionary&va=%s",
    mwt:         "http://www.m-w.com/cgi-bin/thesaurus?book=Thesaurus&va=%s",
    whatis:      "http://uptime.netcraft.com/up/graph/?host=%s",
    wiki:        "http://en.wikipedia.org/w/wiki.phtml?search=%s",
    wikide:      "http://de.wikipedia.org/w/wiki.phtml?search=%s",
    wn:          "http://www.cogsci.princeton.edu/cgi-bin/webwn1.7.1?stage=1&word=%s",
    // rfc by number
    rfc:         "http://www.rfc-editor.org/rfc/rfc%s.txt",
    // rfc search
    rfcs:        "http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    cr:          "http://www.rfc-editor.org/cgi-bin/rfcsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    // Internet Draft search
    rfcid:       "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    urbandict:   "http://www.urbandictionary.com/define.php?term=%s",
    id:          "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
    draft:       "http://www.rfc-editor.org/cgi-bin/idsearch.pl?searchwords=%s&format=http&abstract=abson&keywords=keyon&num=25",
};

function rewrite_uri_classic(uri) {
	var parts = uri.split(" ");
	var prefix = parts[0];
	var rest = parts.slice(1).join(" ");
	var rule = smartprefixes[prefix];

	if (rule) {
		if (typeof(rule) == 'string')
			return rule.replace(/%s/, escape(rest));

		if (typeof(rule) == 'function')
			return rule(rest);

		elinks.alert('smartprefix[' + prefix + ']'
		              + ' has unsupported type "' + t + '".');
	} 
	return uri;
}
elinks.goto_url_hooks.push(rewrite_uri_classic);
