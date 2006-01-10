# Example ~/.elinks/hooks.pl
#
# Copyleft by Russ Rowan (See the file "COPYING" for details.)
#
# To get documentation for this file:
#   pod2html hooks.pl > hooks.html && elinks hooks.html
# or
#   perldoc hooks.pl

=head1 NAME

hooks.pl -- Perl hooks for the ELinks text WWW browser

=head1 DESCRIPTION

This file contains the Perl hooks for the ELinks text WWW browser.

These hooks change the browser's behavior in various ways.  They allow
shortcuts to be used in the Goto URL dialog, modifying the source of a page,
proxy handling, and other things such as displaying a fortune at exit.

=cut
use strict;
use warnings;
use diagnostics;

=head1 CONFIGURATION FILE

This hooks file reads its configuration from I<~/.elinks/config.pl>.
The following is an example of the configuration file:

	bork:       yep       # BORKify Google?
	collapse:   okay      # Collapse all XBEL bookmark folders on exit?
	email:                # Set to show one's own bugs with the "bug" prefix.
	external:   wget      # Send the current URL to this application.
	fortune:    elinks    # *fortune*, *elinks* tip, or *none* on quit?
	googlebeta: hell no   # I miss DejaNews...
	gotosearch: why not   # Anything not a URL in the Goto URL dialog...
	ipv6:       sure      # IPV4 or 6 address blocks with "ip" prefix?
	language:   english   # "bf nl en" still works, but now "bf nl" does too
	news:       msnbc     # Agency to use for "news" and "n" prefixes
	search:     elgoog    # Engine for (search|find|www|web|s|f|go) prefixes
	usenet:     google    # *google* or *standard* view for news:// URLs
	weather:    cnn       # Server for "weather" and "w" prefixes

	# news:    bbc, msnbc, cnn, fox, google, yahoo, reuters, eff, wired,
	#          slashdot, newsforge, usnews, newsci, discover, sciam
	# search:  elgoog, google, yahoo, ask jeeves, a9, altavista, msn, dmoz,
	#          dogpile, mamma, webcrawler, netscape, lycos, hotbot, excite
	# weather: weather underground, google, yahoo, cnn, accuweather,
	#          ask jeeves

I<Developer's usage>: The function I<loadrc()> takes a preference name as its
single argument and returns either an empty string if it is not specified,
I<yes> for a true value (even if specified like I<sure> or I<why not>), I<no>
for a false value (even if like I<nah>, I<off> or I<0>), or the lowercased
preference value (like I<cnn> for C<weather: CNN>).

=cut
sub loadrc($)
{
	my ($preference) = @_;
	my $configperl = $ENV{'HOME'} . '/.elinks/config.pl';
	my $answer = '';

	open RC, "<$configperl" or return $answer;
	while (<RC>)
	{
		s/\s*#.*$//;
		next unless (m/(.*):\s*(.*)/);
		my $setting = $1;
		my $switch = $2;
		next unless ($setting eq $preference);

		if ($switch =~ /^(yes|1|on|yea|yep|sure|ok|okay|yeah|why.*not)$/)
		{
			$answer = "yes";
		}
		elsif ($switch =~ /^(no|0|off|nay|nope|nah|hell.*no)$/)
		{
			$answer = "no";
		}
		else
		{
			$answer = lc($switch);
		}
	}
	close RC;

	return $answer;
}



=head1 GOTO URL HOOK

This is a summary of the shortcuts defined in this file for use in the Goto URL
dialog.  They are similar to the builtin URL prefixes, but more flexible and
powerful.

=over

I<Developer's usage>: The function I<goto_url_hook> is called when the hook is
triggered, taking the target URL and current URL as its two arguments.  It
returns the final target URL.

These routines do a name->URL mapping - for example, the I<goto_url_hook()>
described above maps a certain prefix to C<google> and then asks the
I<search()> mapping routine described below to map the C<google> string to an
appropriate URL.

There are generally two URLs for each name.  One to go to the particular URL's
main page, and another for a search on the given site (if any string is
specified after the prefix).  A few of these prefixes will change their
behavior depending on the URL currently beung displayed in the browser.

=cut
# Don't call them "dumb".  They hate that.  Rather, "interactivity challenged".
################################################################################
### goto_url_hook ##############################################################
sub goto_url_hook
{
	my $url = shift;
	my $current_url = shift;


=item Bugmenot:

B<bugmenot> or B<bn>

=cut
	############################################################################
	# "bugmenot" (no blood today, thank you)
	if ($url =~ '^(bugmenot|bn)$' and $current_url)
	{
		($current_url) = $current_url =~ /^.*:\/\/(.*)/;
		my $bugmenot = 'http://bugmenot.com/view.php?url=' . $current_url;
		#my $tempfile = $ENV{'HOME'} . '/.elinks/elinks';
		#my $matrix = '1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
		#for (0..int(rand(7) + 9))
		#{
			#$tempfile = $tempfile . substr($matrix, (length($matrix) - 1) - rand(length($matrix) + 1), 1);
		#}
		#my ($message, $login, $password);
		#system('elinks -no-home -source "' . $bugmenot . '" >' . $tempfile . ' 2>/dev/null');
		#open FILE, "<$tempfile" or return $bugmenot;
		#$message = <FILE>;
		#while (<FILE>)
		#{
			#next unless (m/^<dd>(.*)<br \/>(.*)<\/dd><\/dl>$/);
			#$login    = $1;
			#$password = $2;
		#}
			#$login    =~ s/(^\s*|\n|\s*$)//g if $login;
			#$password =~ s/(^\s*|\n|\s*$)//g if $password;
		#close FILE;
		#unlink $tempfile;
		#return $bugmenot unless $message =~ /[a-z]+/ and $message !~ /404/;
		#unless ($message =~ s/.*(No accounts found\.).*/${1}/)
		#{
			#if ($login and $password)
			#{
				#$message = "Login: " . $login . "\nPassword: " . $password;
			#}
			#else
			#{
				#$message = 'No accounts found';
			#}
		#}
		#system('elinks -remote "infoBox\(' . $message . ')" >/dev/null 2>&1 &');
		#return $current_url; #FIXME
		##return;
		return $bugmenot . $current_url;
	}


	############################################################################
	# Random URL generator
	if ($url eq 'bored' or $url eq 'random')
	{
		my $word; # You can say *that* again...
		srand();
		open FILE, '</usr/share/dict/words'
			or open FILE, '</usr/share/dict/linux.words'
			or open FILE, '</usr/dict/words'
			or open FILE, '</usr/dict/linux.words'
			or open FILE, '</usr/share/dict/yawl.list'
			or open FILE, $ENV{"HOME"} . '/.elinks/elinks.words'
			or return 'http://google.com/webhp?hl=xx-bork';
		rand($.) < 1 && ($word = $_) while <FILE>;
		close FILE;
		($word) = $word =~ /(.*)/;
		return 'http://' . lc($word) . '.com';
	}


=item Web search:

=over

=item Google:                 B<g> or B<google>  (default)

=item Yahoo:                  B<y> or B<yahoo>

=item Ask Jeeves:             B<ask> or B<jeeves>

=item Amazon A9:              B<a9>

=item Altavista:              B<av> or B<altavista>

=item Microsoft:              B<msn> or B<microsoft>

=item Mozilla Open Directory: B<dmoz>, B<odp>, B<mozilla>

=item Dogpile:                B<dp> or B<dogpile>

=item Mamma:                  B<ma> or B<mamma>

=item Webcrawler:             B<wc> or B<webcrawler>

=item Netscape:               B<ns> or B<netscape>

=item Lycos:                  B<ly> or B<lycos>

=item Hotbot:                 B<hb> or B<hotbot>

=item Excite:                 B<ex> or B<excite>

=item Elgoog:                 B<eg>, B<elgoog>, B<hcraes>, B<dnif>, B<bew>, B<og>

=back

default engine:         B<search>, B<find>, B<www>, B<web>, B<s>, B<f>, B<go>

=over

The I<%search_engines> hash maps each engine name to two URLs, I<home> and
I<search>.  With I<search>, the query is appended to the URL.

The search engines mapping is done by the I<search()> function, taking the
search engine name as its first parameter and optional search string as its
second parameter.  It returns the mapped target URL.

=back

=back

=cut
	############################################################################
	# Search engines
	my %search_prefixes;
	$search_prefixes{'^(g|google)(| .*)$'}                     = 'google';     # Google (default)
	$search_prefixes{'^(y|yahoo)(| .*)$'}                      = 'yahoo';      # Yahoo
	$search_prefixes{'^(ask|jeeves)(| .*)$'}                   = 'ask jeeves'; # Ask Jeeves
	$search_prefixes{'^a9(| .*)$'}                             = 'a9';         # Amazon A9
	$search_prefixes{'^(av|altavista)(| .*)$'}                 = 'altavista';  # Altavista
	$search_prefixes{'^(msn|microsoft)(| .*)$'}                = 'msn';        # Microsoft
	$search_prefixes{'^(dmoz|odp|mozilla)(| .*)$'}             = 'dmoz';       # Mozilla Open Directory
	$search_prefixes{'^(dp|dogpile)(| .*)$'}                   = 'dogpile';    # Dogpile
	$search_prefixes{'^(ma|mamma)(| .*)$'}                     = 'mamma';      # Mamma
	$search_prefixes{'^(wc|webcrawler)(| .*)$'}                = 'webcrawler'; # Webcrawler
	$search_prefixes{'^(ns|netscape)(| .*)$'}                  = 'netscape';   # Netscape
	$search_prefixes{'^(ly|lycos)(| .*)$'}                     = 'lycos';      # Lycos
	$search_prefixes{'^(hb|hotbot)(| .*)$'}                    = 'hotbot';     # Hotbot
	$search_prefixes{'^(ex|excite)(| .*)$'}                    = 'excite';     # Excite
	$search_prefixes{'^(eg|elgoog|hcraes|dnif|bew|og)(| .*)$'} = 'elgoog';     # Elgoog

	sub search
	{
		my %search_engines =
		(
			"elgoog"     => {
				home     => 'http://alltooflat.com/geeky/elgoog/m/index.cgi',
				search   => 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q='},
			"google"     => {
				home     => 'http://google.com!bork!',
				search   => 'http://google.com/search?!bork!q='},
			"yahoo"      => {
				home     => 'http://yahoo.com',
				search   => 'http://search.yahoo.com/search?p='},
			"ask jeeves" => {
				home     => 'http://ask.com',
				search   => 'http://web.ask.com/web?q='},
			"a9"         => {
				home     => 'http://a9.com',
				search   => 'http://a9.com/?q='},
			"altavista"  => {
				home     => 'http://altavista.com',
				search   => 'http://altavista.com/web/results?q='},
			"msn"        => {
				home     => 'http://msn.com',
				search   => 'http://search.msn.com/results.aspx?q='},
			"dmoz"       => {
				home     => 'http://dmoz.org',
				search   => 'http://search.dmoz.org/cgi-bin/search?search='},
			"dogpile"    => {
				home     => 'http://dogpile.com',
				search   => 'http://dogpile.com/info.dogpl/search/web/'},
			"mamma"      => {
				home     => 'http://mamma.com',
				search   => 'http://mamma.com/Mamma?query='},
			"webcrawler" => {
				home     => 'http://webcrawler.com',
				search   => 'http://webcrawler.com/info.wbcrwl/search/web/'},
			"netscape"   => {
				home     => 'http://search.netscape.com',
				search   => 'http://channels.netscape.com/ns/search/default.jsp?query='},
			"lycos"      => {
				home     => 'http://lycos.com',
				search   => 'http://search.lycos.com/default.asp?query='},
			"hotbot"     => {
				home     => 'http://hotbot.com',
				search   => 'http://hotbot.com/default.asp?query='},
			"excite"     => {
				home     => 'http://search.excite.com',
				search   => 'http://search.excite.com/info.xcite/search/web/'},
		);

		my ($engine, $search) = @_;
		my $key = $search ? 'search' : 'home';
		$engine = 'google' unless $search_engines{$engine}
			and $search_engines{$engine}->{$key};
		my $url = $search_engines{$engine}->{$key};
		if ($engine eq 'google')
		{
			my $bork = '';
			if (loadrc('bork') eq 'yes')
			{
				if (not $search)
				{
					$bork = "/webhp?hl=xx-bork";
				}
				else
				{
					$bork = "hl=xx-bork&";
				}
			}
			$url =~ s/!bork!/$bork/;
		}
		if ($search)
		{
			$search =~ s/%/%25/g;
			$search =~ s/&/%26/g;
			$search =~ s/\s/%20/g;
			$search =~ s/\+/%2b/g;
			$search =~ s/#/%23/g;
			$url .= $search;
		}
		return $url;
	}

	my ($search) = $url =~ /^\S+\s+(.*)/;
	if ($url =~ /^(search|find|www|web|s|f|go)(| .*)$/)
	{
		return search(loadrc('search'), $search);
	}
	foreach my $prefix (keys %search_prefixes)
	{
		next unless $url =~ /$prefix/;
		return search($search_prefixes{$prefix}, $search);
	}


=over

=item News agencies:

=over

=item British Broadcasting Corporation: B<bbc>  (default)

=item MSNBC:                            B<msnbc>

=item Cable News Network:               B<cnn>

=item FOXNews:                          B<fox>

=item Google News:                      B<gn>

=item Yahoo News:                       B<yn>

=item Reuters:                          B<rs> or B<reuters>

=item Electronic Frontier Foundation:   B<eff>

=item Wired:                            B<wd> or B<wired>

=item Slashdot:                         B</.> or B<sd> or B<slashdot>

=item NewsForge:                        B<nf> or B<newsforge>

=item U.S.News & World Report:          B<us> or B<usnews>

=item New Scientist:                    B<newsci> or B<nsci>

=item Discover Magazine:                B<dm>

=item Scientific American:              B<sa> or B<sciam>

=back

default agency:                   B<n>, B<news>

=over

The I<%news_servers> hash maps each engine name to two URLs, I<home> and
I<search>.  With I<search>, the query is appended to the mapped URL.

The news servers mapping is done by the I<news()> function, taking the search
engine name as its first parameter and optional search string as its second
parameter.  It returns the mapped target URL.

=back

=back

=cut
	############################################################################
	# News
	my %news_prefixes;
	$news_prefixes{'^bbc(| .*)$'}                = 'bbc';       # British Broadcasting Corporation (default)
	$news_prefixes{'^msnbc(| .*)$'}              = 'msnbc';     # MSNBC
	$news_prefixes{'^cnn(| .*)$'}                = 'cnn';       # Cable News Network
	$news_prefixes{'^fox(| .*)$'}                = 'fox';       # FOXNews
	$news_prefixes{'^gn(| .*)$'}                 = 'google';    # Google News
	$news_prefixes{'^yn(| .*)$'}                 = 'yahoo';     # Yahoo News
	$news_prefixes{'^(reuters|rs)(| .*)$'}       = 'reuters';   # Reuters
	$news_prefixes{'^eff(| .*)$'}                = 'eff';       # Electronic Frontier Foundation
	$news_prefixes{'^(wired|wd)(| .*)$'}         = 'wired';     # Wired
	$news_prefixes{'^(\/\.|slashdot|sd)(| .*)$'} = 'slashdot';  # Slashdot
	$news_prefixes{'^(newsforge|nf)(| .*)$'}     = 'newsforge'; # NewsForge
	$news_prefixes{'^(us|usnews)(| .*)$'}        = 'usnews';    # U.S.News & World Report
	$news_prefixes{'^(nsci|newsci)(| .*)$'}      = 'newsci';    # New Scientist
	$news_prefixes{'^dm(| .*)$'}                 = 'discover';  # Discover Magazine
	$news_prefixes{'^(sa|sciam)(| .*)$'}         = 'sciam';     # Scientific American

	sub news
	{
		my %news_servers =
		(
			"bbc"       => {
				home    => 'http://news.bbc.co.uk',
				search  => 'http://newssearch.bbc.co.uk/cgi-bin/search/results.pl?q='},
			"msnbc"     => { # The bastard child of Microsoft and the National Broadcasting Corporation
				home    => 'http://msnbc.com',
				search  => 'http://msnbc.msn.com/?id=3053419&action=fulltext&querytext='},
			"cnn"       => {
				home    => 'http://cnn.com',
				search  => 'http://search.cnn.com/pages/search.jsp?query='},
			"fox"       => {
				home    => 'http://foxnews.com',
				search  => 'http://search.foxnews.com/info.foxnws/redirs_all.htm?pgtarg=wbsdogpile&qkw='},
			"google"    => {
				home    => 'http://news.google.com',
				search  => 'http://news.google.com/news?q='},
			"yahoo"     => {
				home    => 'http://news.yahoo.com',
				search  => 'http://news.search.yahoo.com/search/news/?p='},
			"reuters"   => {
				home    => 'http://reuters.com',
				search  => 'http://reuters.com/newsSearchResultsHome.jhtml?query='},
			"eff"       => {
				home    => 'http://eff.org',
				search  => 'http://google.com/search?sitesearch=http://eff.org&q='},
			"wired"     => {
				home    => 'http://wired.com',
				search  => 'http://search.wired.com/wnews/default.asp?query='},
			"slashdot"  => {
				home    => 'http://slashdot.org',
				search  => 'http://slashdot.org/search.pl?query='},
			"newsforge" => {
				home    => 'http://newsforge.com',
				search  => 'http://newsforge.com/search.pl?query='},
			"usnews"    => {
				home    => 'http://usnews.com',
				search  => 'http://www.usnews.com/search/Search?keywords='},
			"newsci"    => {
				home    => 'http://newscientist.com',
				search  => 'http://www.newscientist.com/search.ns?doSearch=true&articleQuery.queryString='},
			"discover"  => {
				home    => 'http://discover.com',
				search  => 'http://www.discover.com/search-results/?searchStr='},
			"sciam"     => {
				home    => 'http://sciam.com',
				search  => 'http://sciam.com/search/index.cfm?QT=Q&SC=Q&Q='},
		);

		my ($server, $search) = @_;
		my $key = $search ? 'search' : 'home';
		$server = 'bbc' unless $news_servers{$server}
			and $news_servers{$server}->{$key};
		my $url = $news_servers{$server}->{$key};
		$url .= $search if $search;
		return $url;
	}

	if ($url =~ /^(news|n)(| .*)$/)
	{
		return news(loadrc('news'), $search);
	}
	foreach my $prefix (keys %news_prefixes)
	{
		next unless $url =~ /$prefix/;
		return news($news_prefixes{$prefix}, $search);
	}


=over

=item Locators:

=over

=item Internet Movie Database:            B<imdb>, B<movie>, or B<flick>

=item US zip code search:                 B<zip> or B<usps> (# or address)

=item IP address locator / address space: B<ip>

=item WHOIS / TLD list:                   B<whois> (current url or specified)

=item Request for Comments:               B<rfc> (# or search)

=item Weather:                            B<w> or B<weather>

=item Yahoo! Finance / NASD Regulation:   B<stock>, B<ticker>, or B<quote>

=item Snopes:                             B<ul>, B<urban>, or B<legend>

=item Torrent search / ISOHunt:           B<bt>, B<torrent>, or B<bittorrent>

=item Wayback Machine:                    B<ia>, B<ar>, B<arc>, or B<archive> (current url or specified)

=item Freshmeat:                          B<fm> or B<freshmeat>

=item SourceForge:                        B<sf> or B<sourceforge>

=item Savannah:                           B<sv> or B<savannah>

=item Gna!:                               B<gna>

=item BerliOS:                            B<bl> or B<berlios>

=item Netcraft Uptime Survey:             B<whatis> or B<uptime> (current url or specified)

=item Who's Alive and Who's Dead:         Wanted, B<dead> or B<alive>!

=item Google Library / Project Gutenberg: B<book> or B<read>

=item Internet Public Library:            B<ipl>

=item VIM Tips:                           B<vt> (# or search)

=item Urban Dictionary:                   B<urbandict> or B<ud> <I<word>>

=back

=over

The I<%locators> hash maps each engine name to two URLs, I<home> and I<search>.

B<!current!> string in the URL is substitued for the URL of the current
document.

B<!query!> string in the I<search> URL is substitued for the search string.  If
no B<!query!> string is found in the URL, the query is appended to the mapped
URL.

The locators mapping is done by the I<location()> function, taking the search
engine name as its first parameter, optional search string as its second
parameter and the current document's URL as its third parameter.  It returns
the mapped target URL.

=back

=cut
	############################################################################
	# Locators
	my %locator_prefixes;
	$locator_prefixes{'^(imdb|movie|flick)(| .*)$'}      = 'imdb';        # Internet Movie Database
	$locator_prefixes{'^(stock|ticker|quote)(| .*)$'}    = 'stock';       # Yahoo! Finance / NASD Regulation
	$locator_prefixes{'^(urban|legend|ul)(| .*)$'}       = 'bs';          # Snopes
	$locator_prefixes{'^(bittorrent|torrent|bt)(| .*)$'} = 'torrent';     # Torrent search / ISOHunt
	$locator_prefixes{'^(archive|arc|ar|ia)(| .*)$'}     = 'archive';     # Wayback Machine
	$locator_prefixes{'^(freshmeat|fm)(| .*)$'}          = 'freshmeat';   # Freshmeat
	$locator_prefixes{'^(sourceforge|sf)(| .*)$'}        = 'sourceforge'; # SourceForge
	$locator_prefixes{'^(savannah|sv)(| .*)$'}           = 'savannah';    # Savannah
	$locator_prefixes{'^gna(| .*)$'}                     = 'gna';         # Gna!
	$locator_prefixes{'^(berlios|bl)(| .*)$'}            = 'berlios';     # BerliOS
	$locator_prefixes{'^(alive|dead)(| .*)$'}            = 'dead';        # Who's Alive and Who's Dead
	$locator_prefixes{'^(book|read)(| .*)$'}             = 'book';        # Google Library / Project Gutenberg
	$locator_prefixes{'^ipl(| .*)$'}                     = 'ipl';         # Internet Public Library
	$locator_prefixes{'^(urbandict|ud)(| .*)$'}          = 'urbandict';   # Urban Dictionary
	$locator_prefixes{'^ubs(| .*)$'}                     = 'ubs';         # Usenet binary search

	my %weather_locators =
	(
		'weather underground' => 'http://wunderground.com/cgi-bin/findweather/getForecast?query=!query!',
		'google'              => 'http://google.com/search?q=weather+"!query!"',
		'yahoo'               => 'http://search.yahoo.com/search?p=weather+"!query!"',
		'cnn'                 => 'http://weather.cnn.com/weather/search?wsearch=!query!',
		'accuweather'         => 'http://wwwa.accuweather.com/adcbin/public/us_getcity.asp?zipcode=!query!',
		'ask jeeves'          => 'http://web.ask.com/web?&q=weather !query!',
	);

	sub location
	{
		my %locators =
		(
			'imdb'        => {
				home      => 'http://imdb.com',
				search    => 'http://imdb.com/Find?select=All&for='},
			'stock'       => {
				home      => 'http://nasdr.com',
				search    => 'http://finance.yahoo.com/l?s='},
			'bs'          => {
				home      => 'http://snopes.com',
				search    => 'http://search.atomz.com/search/?sp-a=00062d45-sp00000000&sp-q='},
			'torrent'     => {
				home      => 'http://isohunt.com',
				search    => 'http://google.com/search?q=filetype:torrent !query!!bork!'},
			'archive'     => {
				home      => 'http://web.archive.org/web/*/!current!',
				search    => 'http://web.archive.org/web/*/'},
			'freshmeat'   => {
				home      => 'http://freshmeat.net',
				search    => 'http://freshmeat.net/search/?q='},
			'sourceforge' => {
				home      => 'http://sourceforge.net',
				search    => 'http://sourceforge.net/search/?q='},
			'savannah'    => {
				home      => 'http://savannah.nongnu.org',
				search    => 'http://savannah.nongnu.org/search/?type_of_search=soft&words='},
			'gna'         => {
				home      => 'http://gna.org',
				search    => 'https://gna.org/search/?type_of_search=soft&words='},
			'berlios'     => {
				home      => 'http://www.berlios.de',
				search    => 'http://developer.berlios.de/search/?type_of_search=soft&words='},
			'dead'        => {
				home      => 'http://www.whosaliveandwhosdead.com',
				search    => 'http://google.com/search?btnI&sitesearch=http://whosaliveandwhosdead.com&q='},
			'book'        => {
				home      => 'http://gutenberg.org',
				search    => 'http://google.com/search?q=book+"!query!"'},
			'ipl'         => {
				home      => 'http://ipl.org',
				search    => 'http://ipl.org/div/searchresults/?words='},
			'urbandict'   => {
				home      => 'http://urbandictionary.com/random.php',
				search    => 'http://urbandictionary.com/define.php?term='},
			'ubs'         => {
				home      => 'http://binsearch.info',
				search    => 'http://binsearch.info/?q='},
		);

		my ($server, $search, $current_url) = @_;
		my $key = $search ? 'search' : 'home';
		return unless $locators{$server} and $locators{$server}->{$key};
		my $url = $locators{$server}->{$key};
		my $bork = ""; $bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
		$url =~ s/!bork!/$bork/g;
		$url =~ s/!current!/$current_url/g;
		$url .= $search if $search and not $url =~ s/!query!/$search/g;
		return $url;
	}

	foreach my $prefix (keys %locator_prefixes)
	{
		next unless $url =~ /$prefix/;
		return location($locator_prefixes{$prefix}, $search, $current_url);
	}

	if ($url =~ '^(zip|usps)(| .*)$'
		or $url =~ '^ip(| .*)$'
		or $url =~ '^whois(| .*)$'
		or $url =~ '^rfc(| .*)$'
		or $url =~ '^(weather|w)(| .*)$'
		or $url =~ '^(whatis|uptime)(| .*)$'
		or $url =~ '^vt(| .*)$')
	{
		my ($thingy) = $url =~ /^[a-z]* (.*)/;
		my ($domain) = $current_url =~ /([a-z0-9-]+\.(com|net|org|edu|gov|mil))/;

		my $locator_zip          = 'http://usps.com';
		my $ipv                  = "ipv4-address-space"; $ipv = "ipv6-address-space" if loadrc("ipv6") eq "yes";
			my $locator_ip       = 'http://www.iana.org/assignments/' . $ipv;
		my $whois                = 'http://reports.internic.net/cgi/whois?type=domain&whois_nic=';
			my $locator_whois    = 'http://www.iana.org/cctld/cctld-whois.htm';
			$locator_whois       = $whois . $domain if $domain;
		my $locator_rfc          = 'http://ietf.org';
		my $locator_weather      = 'http://weather.noaa.gov';
		my $locator_whatis       = 'http://uptime.netcraft.com';
			$locator_whatis      = 'http://uptime.netcraft.com/up/graph/?host=' . $domain if $domain;
		my $locator_vim          = 'http://www.vim.org/tips';
		if ($thingy)
		{
			$locator_zip         = 'http://zip4.usps.com/zip4/zip_responseA.jsp?zipcode=' . $thingy;
				$locator_zip     = 'http://zipinfo.com/cgi-local/zipsrch.exe?zip=' . $thingy if $thingy !~ '^[0-9]*$';
			$locator_ip          = 'http://melissadata.com/lookups/iplocation.asp?ipaddress=' . $thingy;
			$locator_whois       = $whois . $thingy;
			$locator_rfc         = 'http://rfc-editor.org/cgi-bin/rfcsearch.pl?num=37&searchwords=' . $thingy;
				$locator_rfc     = 'http://ietf.org/rfc/rfc' . $thingy . '.txt' unless $thingy !~ '^[0-9]*$';
			my $weather          = loadrc("weather");
				$locator_weather = $weather_locators{$weather};
				$locator_weather ||= $weather_locators{'weather underground'};
				$locator_weather =~ s/!query!/$thingy/;
			$locator_whatis      = 'http://uptime.netcraft.com/up/graph/?host=' . $thingy;
			$locator_vim         = 'http://www.vim.org/tips/tip_search_results.php?order_by=rating&keywords=' . $thingy;
				$locator_vim     = 'http://www.vim.org/tips/tip.php?tip_id=' . $thingy unless $thingy !~ '^[0-9]*$';
		}
		return $locator_zip     if ($url =~ '^(zip|usps)(| .*)$');
		return $locator_ip      if ($url =~ '^ip(| .*)$');
		return $locator_whois   if ($url =~ '^whois(| .*)$');
		return $locator_rfc     if ($url =~ '^rfc(| .*)$');
		return $locator_weather if ($url =~ '^(weather|w)(| .*)$');
		return $locator_whatis  if ($url =~ '^(whatis|uptime)(| .*)$');
		return $locator_vim     if ($url =~ '^vt(| .*)$');
	}


=item Google Groups:

B<deja>, B<gg>, B<groups>, B<gr>, B<nntp>, B<usenet>, B<nn>

=cut
	############################################################################
	# Google Groups (DejaNews)
	if ($url =~ '^(deja|gg|groups|gr|nntp|usenet|nn)(| .*)$')
	{
		my ($search) = $url =~ /^[a-z]* (.*)/;
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") ne "yes");
		my $bork = "";
		if ($search)
		{
			$bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
			my ($msgid) = $search =~ /^<(.*)>$/;
			return 'http://' . $beta . '/groups?as_umsgid=' . $msgid . $bork if $msgid;
			return 'http://' . $beta . '/groups?q=' . $search . $bork;
		}
		else
		{
			$bork = "/groups?hl=xx-bork" unless (loadrc("bork") ne "yes");
			return 'http://' . $beta . $bork;
		}
	}


=item MirrorDot:

B<md> or B<mirrordot> <I<URL>>

=cut
	############################################################################
	# MirrorDot
	if ($url =~ '^(mirrordot|md)(| .*)$')
	{
		my ($slashdotted) = $url =~ /^[a-z]* (.*)/;
		if ($slashdotted)
		{
			return 'http://mirrordot.com/find-mirror.html?' . $slashdotted;
		}
		else
		{
			return 'http://mirrordot.com';
		}
	}


	############################################################################
	# The Bastard Operator from Hell
	if ($url =~ '^bofh$')
	{
		return 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
	}


=item Coral cache:

B<cc>, B<coral>, or B<nyud> <I<URL>>

=cut
	############################################################################
	# Coral cache <URL>
	if ($url =~ '^(coral|cc|nyud)( .*)$')
	{
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		return 'http://' . $cache;
	}


=item AltaVista Babelfish:

B<babelfish>, B<babel>, B<bf>, B<translate>, B<trans>, or B<b> <I<from>> <I<to>>

"babelfish german english" or "bf de en"

=cut
	############################################################################
	# AltaVista Babelfish ("babelfish german english"  or  "bf de en")
	if (($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]* [a-zA-Z]*)$')
		or ($url =~ '^(babelfish|babel|bf|translate|trans|b)(| [a-zA-Z]*(| [a-zA-Z]*))$'
		and loadrc("language") and $current_url))
	{
		$url = 'http://babelfish.altavista.com' if ($url =~ /^[a-z]*$/);
		if ($url =~ /^[a-z]* /)
		{
			my $tongue = loadrc("language");
			$url = $url . " " . $tongue if ($tongue ne "no" and $url !~ /^[a-z]* [a-zA-Z]* [a-zA-Z]*$/);
			$url =~ s/ chinese/ zt/i;
			$url =~ s/ dutch/ nl/i;
			$url =~ s/ english/ en/i;
			$url =~ s/ french/ fr/i;
			$url =~ s/ german/ de/i;
			$url =~ s/ greek/ el/i;
			$url =~ s/ italian/ it/i;
			$url =~ s/ japanese/ ja/i;
			$url =~ s/ korean/ ko/i;
			$url =~ s/ portugese/ pt/i;
			$url =~ s/ russian/ ru/i;
			$url =~ s/ spanish/ es/i;
			my ($from_language, $to_language) = $url =~ /^[a-z]* (.*) (.*)$/;
			($current_url) = $current_url =~ /^.*:\/\/(.*)/;
			$url = 'http://babelfish.altavista.com/babelfish/urltrurl?lp='
				. $from_language . '_' . $to_language . '&url=http%3A%2F%2F' . $current_url;
		}
		return $url;
	}


	############################################################################
	# XYZZY
	if ($url =~ '^xyzzy$')
	{
		# $url = 'http://sundae.triumf.ca/pub2/cave/node001.html';
		srand();
		my $yzzyx;
		my $xyzzy = int(rand(8));
		$yzzyx = 1   if ($xyzzy == 0); # Colossal Cave Adventure
		$yzzyx = 2   if ($xyzzy == 1); # Dungeon
		$yzzyx = 227 if ($xyzzy == 2); # Zork Zero: The Revenge of Megaboz
		$yzzyx = 3   if ($xyzzy == 3); # Zork I: The Great Underground Empire
		$yzzyx = 4   if ($xyzzy == 4); # Zork II: The Wizard of Frobozz
		$yzzyx = 5   if ($xyzzy == 5); # Zork III: The Dungeon Master
		$yzzyx = 6   if ($xyzzy == 6); # Zork: The Undiscovered Underground
		$yzzyx = 249 if ($xyzzy == 7); # Hunt the Wumpus
		return 'http://ifiction.org/games/play.php?game=' . $yzzyx;
	}


	############################################################################
	# ...and now, Deep Thoughts.  by Jack Handey
	if ($url =~ '^(jack|handey)$')
	{
		return 'http://glug.com/handey';
	}


=item W3C page validators:

B<vhtml> or B<vcss> <I<URL>> (or current url)

=cut
	############################################################################
	# Page validators [<URL>]
	if ($url =~ '^vhtml(| .*)$' or $url =~ '^vcss(| .*)$')
	{
		my ($page) = $url =~ /^.* (.*)/;
		$page = $current_url unless $page;
		return 'http://validator.w3.org/check?uri=' . $page if $url =~ 'html';
		return 'http://jigsaw.w3.org/css-validator/validator?uri=' . $page if $url =~ 'css';
	}


=item ELinks:

=over

=item Home:                  B<el> or B<elinks>

=item Bugzilla:              B<bz> or B<bug> (# or search optional)

=item Documentation and FAQ: B<doc(|s|umentation)> or B<faq>

=back

There's no place like home...

=cut
	############################################################################
	# There's no place like home
	if ($url =~ '^(el(|inks)|b(ug(|s)|z)(| .*)|doc(|umentation|s)|faq|help|manual)$')
	{
		my ($bug) = $url =~ /^.* (.*)/;
		if ($url =~ '^b')
		{
			my $bugzilla = 'http://bugzilla.elinks.cz';
			if (not $bug)
			{
				if (loadrc("email"))
				{
					$bugzilla = $bugzilla .
						'/buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&email1='
						. loadrc("email") . '&emailtype1=exact&emailassigned_to1=1&emailreporter1=1';
				}
				return $bugzilla;
			}
			elsif ($bug =~ '^[0-9]*$')
			{
				return $bugzilla . '/show_bug.cgi?id=' . $bug;
			}
			else
			{
				return $bugzilla . '/buglist.cgi?short_desc_type=allwordssubstr&short_desc=' . $bug;
			}
		}
		else
		{
			my $doc = '';
			$doc = '/documentation' if $url =~ '^doc';
			$doc = '/faq.html' if $url =~ '^(faq|help)$';
			$doc = '/documentation/html/manual.html' if $url =~ '^manual$';
			return 'http://elinks.cz' . $doc;
		}
	}


=item The Dialectizer:

B<dia> <I<dialect>> <I<URL>> (or current url)

Dialects: I<redneck>, I<jive>, I<cockney>, I<fudd>, I<bork>, I<moron>, I<piglatin>, or I<hacker>

=cut
	############################################################################
	# the Dialectizer (dia <dialect> <url>)
	if ($url =~ '^dia(| [a-z]*(| .*))$')
	{
		my ($dialect) = $url =~ /^dia ([a-z]*)/;
			$dialect = "hckr" if $dialect and $dialect eq 'hacker';
		my ($victim) = $url =~ /^dia [a-z]* (.*)$/;
			$victim = $current_url if (!$victim and $current_url and $dialect);
		$url = 'http://rinkworks.com/dialect';
		if ($dialect and $dialect =~ '^(redneck|jive|cockney|fudd|bork|moron|piglatin|hckr)$' and $victim)
		{
			$victim =~ s/^http:\/\///;
			$url = $url . '/dialectp.cgi?dialect=' . $dialect . '&url=http%3a%2f%2f' . $victim . '&inside=1';
		}
		return $url;
	}


=item Sender:

B<send>

=over

Send the current URL to the application specified by the configuration variable
'I<external>'.  Optionally, override this by specifying the application as in
'I<send> <I<application>>'.

=back

=cut
	############################################################################
	# send the current URL to another application
	if ($url =~ '^send(| .*)$' and $current_url)
	{
		my ($external) = $url =~ /^send (.*)/;
		if ($external)
		{
			system($external . ' "' . $current_url . '" 2>/dev/null &');
			return $current_url; #FIXME
			#return;
		}
		else
		{
			if (loadrc("external"))
			{
				system(loadrc("external") . ' "' . $current_url . '" 2>/dev/null &');
				return $current_url; #FIXME
				#return;
			}
		}
	}


=item Dictionary:

B<dict>, B<d>, B<def>, or B<define> <I<word>>

=cut
	############################################################################
	# Dictionary
	if ($url =~ '^(dict|d|def|define)(| .*)$')
	{
		my $dict = 'http://dict.org/bin/Dict?Form=Dict1&Strategy=*&Database=*&Query=';
		my ($word) = $url =~ /^[a-z]* (.*)/;
		unless ($word)
		{
			open FILE, '</usr/share/dict/words'
				or open FILE, '</usr/share/dict/linux.words'
				or open FILE, '</usr/dict/words'
				or open FILE, '</usr/dict/linux.words'
				or open FILE, '</usr/share/dict/yawl.list'
				or return 'http://ypass.net/dictionary/index.html?random=1';
			rand($.) < 1 && ($word = $_) while <FILE>;
			close FILE;
		}
		return $dict . $word;
	}


=item Google site search

B<ss> <I<domain>> <I<string>>

=over

Use Google to search the current site or a specified site.  If a domain is not
given, use the current one.

=back

=cut
	############################################################################
	# Google site search
	if ($url =~ '^ss(| .*)$')
	{
		my ($site, $search) = $url =~ /^ss\s(\S+)\s(.*)/;
		if (isurl($site) =~ 'false')
		{
			$search = $site . $search if $site;
			$site = undef;
		}
		unless ($site and $search)
		{
			($search) = $url =~ /^ss\s(.*)/;
			$site = $current_url if $current_url and $current_url !~ '^file://';
		}
		if ($site and $search and $site ne 1 and $search ne 1)
		{
			return 'http://google.com/search?sitesearch=' . $site . '&q=' . $search;
		}
		return $current_url; #FIXME
		#return;
	}


=item Anything not a prefix, URL, or local file will be treated as a search
using the search engine defined by the 'search' configuration option if
'gotosearch' is set to some variation of 'yes'.

=cut
	############################################################################
	# Anything not otherwise useful is a search
	if ($current_url and loadrc("gotosearch") eq "yes")
	{
		return search(loadrc("search"), $url) if isurl($url) =~ 'false';
	}


	return $url;
}


=back


=head1 FOLLOW URL HOOK

These hooks effect a URL before ELinks has a chance to load it.

=over

I<Developer's usage>: The function I<follow_url_hook> is called when the hook
is triggered, taking the target URL as its only argument.  It returns the final
target URL.

=cut
################################################################################
### follow_url_hook ############################################################
sub follow_url_hook
{
	my $url = shift;

=item Bork! Bork! Bork!

Rewrites many I<google.com> URLs.

=cut
	# Bork! Bork! Bork!
	if ($url =~ 'google\.com' and loadrc("bork") eq "yes")
	{
		if ($url =~ '^http://(|www\.|search\.)google\.com(|/search)(|/)$')
		{
			return 'http://google.com/webhp?hl=xx-bork';
		}
		elsif ($url =~ '^http://(|www\.)groups\.google\.com(|/groups)(|/)$'
			or $url =~ '^http://(|www\.|search\.)google\.com/groups(|/)$')
		{
			return 'http://google.com/groups?hl=xx-bork';
		}
	}

=item NNTP over Google

Translates any I<nntp:> or I<news:> URLs to Google Groups HTTP URLs.

=cut
	# nntp?  try Google Groups
	if ($url =~ '^(nntp|news):' and loadrc("usenet") ne "standard")
	{
		my $beta = "groups.google.co.uk";
		$beta = "groups-beta.google.com" unless (loadrc("googlebeta") ne "yes");
		$url =~ s/\///g;
		my ($group) = $url =~ /[a-zA-Z]:(.*)/;
		my $bork = "";
		$bork = "hl=xx-bork&" unless (loadrc("bork") ne "yes");
		return 'http://' . $beta . '/groups?' . $bork . 'group=' . $group;
	}

	# strip trailing spaces
	$url =~ s/\s*$//;

	return $url;
}


=back


=head1 PRE FORMAT HTML HOOK

When an HTML document is downloaded and is about to undergo the final
rendering, this hook is called.  This is frequently used to get rid of ads, but
also various ELinks-unfriendly HTML code and HTML snippets which are irrelevant
to ELinks but can obfuscate the rendered document.

Note that these hooks are applied B<only> before the final rendering, not
before the gradual re-renderings which happen when only part of the document is
available.

=over

I<Developer's usage>: The function I<pre_format_html_hook> is called when the
hook is triggered, taking the document's URL and the HTML source as its two
arguments.  It returns the rewritten HTML code.

=cut
################################################################################
### pre_format_html_hook #######################################################
sub pre_format_html_hook
{
	my $url = shift;
	my $html = shift;
#	my $content_type = shift;


=item Slashdot Sanitation

Kills Slashdot's Advertisements.  (This one is disabled due to weird behavior
with fragments.)

=cut
	# /. sanitation
	if ($url =~ 'slashdot\.org')
	{
	#	$html =~ s/^<!-- Advertisement code. -->.*<!-- end ad code -->$//sm;
	#	$html =~ s/<iframe.*><\/iframe>//g;
	#	$html =~ s/<B>Advertisement<\/B>//;
	}


=item Obvious Google Tips Annihilator

Kills some irritating Google tips.

=cut
	# yes, I heard you the first time
	if ($url =~ 'google\.com')
	{
		$html =~ s/Teep: In must broosers yuoo cun joost heet zee retoorn key insteed ooff cleecking oon zee seerch boottun\. Bork bork bork!//;
		$html =~ s/Tip:<\/font> Save time by hitting the return key instead of clicking on "search"/<\/font>/;
	}


=item SourceForge AdSmasher

Wipes out SourceForge's Ads.

=cut
	# SourceForge ad smasher
	if ($url =~ 'sourceforge\.net')
	{
		$html =~ s/<!-- AD POSITION \d+ -->.*?<!-- END AD POSITION \d+ -->//smg;
		$html =~ s/<b>&nbsp\;&nbsp\;&nbsp\;Site Sponsors<\/b>//g;
	}


=item Gmail's Experience

Gmail has obviously never met ELinks...

=cut
	# Gmail has obviously never met ELinks
	if ($url =~ 'gmail\.google\.com')
	{
		$html =~ s/^<b>For a better Gmail experience, use a.+?Learn more<\/a><\/b>$//sm;
	}


=item Source readability improvements

Rewrites some evil characters to entities and vice versa.  These will be
disabled until such time as pre_format_html_hook only gets called for
content-type:text/html.

=cut
	# demoronizer
#	if ($content_type =~ 'text/html')
#	{
#		$html =~ s/Ñ/\&mdash;/g;
#		$html =~ s/\&#252/ü/g;
#		$html =~ s/\&#039(?!;)/'/g;
#		$html =~ s/]\n>$//gsm;
		#$html =~ s/%5B/[/g;
		#$html =~ s/%5D/]/g;
		#$html =~ s/%20/ /g;
		#$html =~ s/%2F/\//g;
		#$html =~ s/%23/#/g;
#	}


	return $html;
}


=back


=head1 PROXY FOR HOOK

The Perl hooks are asked whether to use a proxy for a given URL (or what proxy
to actually use).  You can use it if you don't want to use a proxy for certain
Intranet servers but you need to use it in order to get to the Internet, or if
you want to use some anonymizer for access to certain sites.

=over

I<Developer's usage>: The function I<proxy_for_hook> is called when the hook is
triggered, taking the target URL as its only argument.  It returns the proxy
URL, empty string to use no proxy or I<undef> to use the default proxy URL.

=cut
################################################################################
### proxy_for_hook #############################################################
sub proxy_for_hook
{
	my $url = shift;


=item No proxy for local files

Prevents proxy usage for local files and C<http://localhost>.

=cut
	# no proxy for local files
	if ($url =~ '^(file://|(http://|)(localhost|127\.0\.0\.1)(/|:|$))')
	{
		return "";
	}

	return;
}


=back


=head1 QUIT HOOK

The Perl hooks can also perform various actions when ELinks quits.  These can
be things like retouching the just saved "information files", or doing some fun
stuff.

=over

I<Developer's usage>: The function I<quit_hook> is called when the hook is
triggered, taking no arguments nor returning anything.  ('cause, you know, what
would be the point?)

=cut
################################################################################
### quit_hook ##################################################################
sub quit_hook
{


=item Collapse XBEL Folders

Collapses XBEL bookmark folders.  This is obsoleted by
I<bookmarks.folder_state>.

=cut
	# collapse XBEL bookmark folders (obsoleted by bookmarks.folder_state)
	my $bookmarkfile = $ENV{'HOME'} . '/.elinks/bookmarks.xbel';
	if (-f $bookmarkfile and loadrc('collapse') eq 'yes')
	{
		open BOOKMARKS, "+<$bookmarkfile";
		my $bookmark;
		while (<BOOKMARKS>)
		{
			s/<folder folded="no">/<folder folded="yes">/;
			$bookmark .= $_;
		}
		seek(BOOKMARKS, 0, 0);
		print BOOKMARKS $bookmark;
		truncate(BOOKMARKS, tell(BOOKMARKS));
		close BOOKMARKS;
	}


=item Words of Wisdom

A few words of wisdom from ELinks the Sage.

=cut
	# words of wisdom from ELinks the Sage
	if (loadrc('fortune') eq 'fortune')
	{
		system('echo ""; fortune -sa 2>/dev/null');
		die;
	}
	die if (loadrc('fortune') =~ /^(none|quiet)$/);
	my $cookiejar = 'elinks.fortune';
	my $ohwhynot = `ls /usr/share/doc/elinks*/$cookiejar 2>/dev/null`;
	open COOKIES, $ENV{"HOME"} . '/.elinks/' . $cookiejar
		or open COOKIES, '/etc/elinks/' . $cookiejar
		or open COOKIES, '/usr/share/elinks/' . $cookiejar
		or open COOKIES, $ohwhynot
		or die system('echo ""; fortune -sa 2>/dev/null');
	my (@line, $fortune);
	$line[0] = 0;
	while (<COOKIES>)
	{
		$line[$#line + 1] = tell if /^%$/;
	}
	srand();
	while (not $fortune)
	{
		seek(COOKIES, $line[int rand($#line + 1)], 0);
		while (<COOKIES>)
		{
			last if /^%$/;
			$fortune .= $_;
		}
	}
	close COOKIES;
	print "\n", $fortune;
}


=back


=head1 SEE ALSO

elinks(1), perl(1)


=head1 AUTHORS

Russ Rowan, Petr Baudis

=cut



sub isurl
{
	my ($url) = @_;
	return 'false' if not $url;
	opendir(DIR, '.');
	my @files = readdir(DIR);
	closedir(DIR);
	foreach my $file (@files)
	{
		return 'true' if $url eq $file;
	}
	return 'true' if $url =~ /^(\/|~)/;
	return 'true' if $url =~ /([0-9]{1,3}\.){3}[0-9]{1,3}($|\/|\?|:[0-9]{1,5})/;
	return 'true' if $url =~ /^((::|)[[:xdigit:]]{1,4}(:|::|)){1,8}($|\/|\?|:[0-9]{1,5})/ and $url =~ /:/;
	if (     $url =~ /^(([a-zA-Z]{3,}(|4|6):\/\/|(www|ftp)\.)|)[a-zA-Z0-9]+/
		and ($url =~ /[a-zA-Z0-9-]+\.(com|org|net|edu|gov|int|mil)($|\/|\?|:[0-9]{1,5})/
		or   $url =~ /[a-zA-Z0-9-]+\.(biz|info|name|pro|aero|coop|museum)($|\/|\?|:[0-9]{1,5})/
		or   $url =~ /[a-zA-Z0-9-]+\.[a-zA-Z]{2}($|\/|\?|:[0-9]{1,5})/))
	{
		return 'true';
	}
	return 'true' if $url =~ /^about:/;

	return 'false';
}



# vim: ts=4 sw=4 sts=0 nowrap
