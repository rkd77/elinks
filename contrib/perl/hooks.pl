# Example hooks.pl file, put in ~/.elinks/ as hooks.pl.
# $Id: hooks.pl,v 1.2.2.4 2005/05/01 21:22:02 jonas Exp $
#
# This file is (c) Russ Rowan and Petr Baudis and GPL'd.
#
# To get the complete user documentation for this file in a user-friendly
# manner, do either
#	pod2html hooks.pl > hooks.html && elinks hooks.html
# or
#	perldoc hooks.pl


=head1 NAME

hooks.pl -- Perl hooks for the ELinks text WWW browser

=head1 DESCRIPTION

This file contains the Perl hooks for the ELinks text WWW browser.

These hooks can alter the browser's behaviour in various ways; probably the
most popular is processing the user input in the Goto URL dialog and rewriting
it in various ways (like the builtin URL prefix capability, but much more
powerful and flexible).

Another popular capability of the hooks is to rewrite the HTML source of the page
before it is rendered, usually to get rid of ads and/or make the web page
more ELinks-friendly.  The hooks also allow you to fine-tune the proxying rules,
can show a fortune when ELinks exits, and more!

=cut


use strict;
use warnings;
use diagnostics;

use Carp;


=head1 CONFIGURATION FILE

The hooks file reads its configuration from I<~/.elinks/config.pl>.
Note that the following is only an example,
and does not contain the default values:

	bork:       yep       # BORKify Google?
	collapse:   okay      # Collapse all XBEL bookmark folders on exit?
	email:                # Set to show one's own bugs with the "bug" prefix.
	fortune:    elinks    # *fortune*, *elinks* tip, or *none* on quit?
	googlebeta: hell no   # I miss DejaNews...
	gotosearch: not yet   # Don't use this yet.  It's broken.
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
I<yes> for a true value (even if specified like I<sure> or I<why not>),
I<no> for a false value (even if like I<nah>, I<off> or I<0>),
or the lowercased preference value (like I<cnn> for C<weather: CNN>).

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


#Don't call the prefixes "dumb", they hate that!  Rather, "interactivity
#challenged".  (Such politically correct names always appeared to me to be
#so much more insulting... --pasky ;-)

=head1 GOTO URL REWRITES

Here you can find summary of all the available rewrites of what you type into
the Goto URL box.  They are similar in spirit to the builtin URI rewrites, but
more flexible and immensely more powerful.

I<Developer's usage>: The function I<goto_url_hook> is called when the hook
is triggered, taking the target URI and current URI as its two arguments.  It
returns the final target URI.

These are the default rewrite rules:

=head2 Miscellaneous shortcuts

B<bugmenot>, B<bored>, B<random>, B<bofh>, B<xyzzy>, B<jack> or B<handey>

=over 4

=item Google Groups

B<deja>, B<gg>, B<groups>, B<gr>, B<nntp>, B<usenet>, B<nn>

=item AltaVista Babelfish

B<babelfish>, B<babel>, B<bf>, B<translate>, B<trans>, or B<b>

=item MirrorDot

B<md> or B<mirrordot>

=item Coral cache

B<cc>, B<coral>, or B<nyud> (requires URL)

=item W3C page validators

B<vhtml> or B<vcss> (current url or specified)

=item The Dialectizer

B<dia> <dialect> (current url or specified)

Dialects can be:
I<redneck>, I<jive>, I<cockney>, I<fudd>, I<bork>, I<moron>, I<piglatin>, or I<hacker>

=back


=head2 Web search

=cut

#######################################################################

my %search_prefixes;

=over 4

=item Default engine

B<search>, B<find>, B<www>, B<web>, B<s>, B<f>, B<go>,
and anything in quotes with no prefix.

=item Google

B<g> or B<google>

=cut

$search_prefixes{'^(g|google)(| .*)$'} = 'google';

=item Yahoo

B<y> or B<yahoo>

=cut

$search_prefixes{'^(y|yahoo)(| .*)$'} = 'yahoo';

=item Ask Jeeves

B<ask> or B<jeeves>

=cut

$search_prefixes{'^(ask|jeeves)(| .*)$'} = 'ask jeeves';

=item Amazon/A9

B<a9>

=cut

$search_prefixes{'^a9(| .*)$'} = 'a9';

=item Altavista

B<av> or B<altavista>

=cut

$search_prefixes{'^(av|altavista)(| .*)$'} = 'altavista';

=item Microsoft

B<msn> or B<microsoft>

=cut

$search_prefixes{'^(msn|microsoft)(| .*)$'} = 'msn';

=item dmoz

B<dmoz>, B<odp>, B<mozilla>

=cut

$search_prefixes{'^(dmoz|odp|mozilla)(| .*)$'} = 'dmoz';

=item Dogpile

B<dp> or B<dogpile>

=cut

$search_prefixes{'^(dp|dogpile)(| .*)$'} = 'dogpile';

=item Mamma

B<ma> or B<mamma>

=cut

$search_prefixes{'^(ma|mamma)(| .*)$'} = 'mamma';

=item Webcrawler

B<wc> or B<webcrawler>

=cut

$search_prefixes{'^(wc|webcrawler)(| .*)$'} = 'webcrawler';

=item Netscape

B<ns> or B<netscape>

=cut

$search_prefixes{'^(ns|netscape)(| .*)$'} = 'netscape';

=item Lycos

B<ly> or B<lycos>

=cut

$search_prefixes{'^(ly|lycos)(| .*)$'} = 'lycos';

=item Hotbot

B<hb> or B<hotbot>

=cut

$search_prefixes{'^(hb|hotbot)(| .*)$'} = 'hotbot';

=item Excite

B<ex> or B<excite>

=cut

$search_prefixes{'^(ex|excite)(| .*)$'} = 'excite';

=item Elgoog

B<eg>, B<elgoog>, B<hcraes>, B<dnif>, B<bew>, B<og>

=cut

$search_prefixes{'^(eg|elgoog|hcraes|dnif|bew|og)(| .*)$'} = 'elgoog';


#######################################################################

=back



=head2 News agencies

=cut

my %news_prefixes;

=over 4

=item Default agency

B<n>, B<news>

=item British Broadcasting Corporation

B<bbc>

=cut
$news_prefixes{'^bbc(| .*)$'} = 'bbc';

=item MSNBC

B<msnbc>

=cut
$news_prefixes{'^msnbc(| .*)$'} = 'msnbc';

=item Cable News Network

B<cnn>

=cut
$news_prefixes{'^cnn(| .*)$'} = 'cnn';

=item FOXNews

B<fox>

=cut
$news_prefixes{'^fox(| .*)$'} = 'fox';

=item Google News

B<gn>

=cut
$news_prefixes{'^gn(| .*)$'} = 'google';

=item Yahoo News

B<yn>

=cut
$news_prefixes{'^yn(| .*)$'} = 'yahoo';

=item Reuters

B<rs> or B<reuters>

=cut
$news_prefixes{'^(reuters|rs)(| .*)$'} = 'reuters';

=item Electronic Frontier Foundation

B<eff>

=cut
$news_prefixes{'^eff(| .*)$'} = 'eff';

=item Wired

B<wd> or B<wired>

=cut
$news_prefixes{'^(wired|wd)(| .*)$'} = 'wired';

=item Slashdot

B</.> or B<sd> or B<slashdot>

=cut
$news_prefixes{'^(\/\.|slashdot|sd)(| .*)$'} = 'slashdot';

=item NewsForge

B<nf> or B<newsforge>

=cut
$news_prefixes{'^(newsforge|nf)(| .*)$'} = 'newsforge';

=item U.S.News & World Report

B<us> or B<usnews>

=cut
$news_prefixes{'^(us|usnews)(| .*)$'} = 'usnews';

=item New Scientist

B<newsci> or B<nsci>

=cut
$news_prefixes{'^(nsci|newsci)(| .*)$'} = 'newsci';

=item Discover Magazine

B<dm>

=cut
$news_prefixes{'^dm(| .*)$'} = 'discover';

=item Scientific American

B<sa> or B<sciam>

=cut
$news_prefixes{'^(sa|sciam)(| .*)$'} = 'sciam';


#######################################################################

=back


=head2 Locators

=cut

# Some of those are handled specially and not in this hash.
my %locator_prefixes;

=over 4

=item Internet Movie Database

B<imdb>, B<movie>, or B<flick>

=cut
$locator_prefixes{'^(imdb|movie|flick)(| .*)$'} = 'imdb';

=item US zip code search

B<zip> or B<usps> (# or address)

=cut

=item IP address locator / address space

B<ip>

=cut

=item WHOIS / TLD list

B<whois> (current url or specified)

=cut

=item Request for Comments

B<rfc> (# or search)

=cut

=item Weather

B<w> or B<weather>

=cut

=item Yahoo! Finance / NASD Regulation

B<stock>, B<ticker>, or B<quote>

=cut
$locator_prefixes{'^(stock|ticker|quote)(| .*)$'} = 'stock';

=item Snopes

B<ul>, B<urban>, or B<legend>

=cut
$locator_prefixes{'^(urban|legend|ul)(| .*)$'} = 'bs';

=item Torrent search / ISOHunt

B<bt>, B<torrent>, or B<bittorrent>

=cut
$locator_prefixes{'^(bittorrent|torrent|bt)(| .*)$'} = 'torrent';

=item Wayback Machine

B<ia>, B<ar>, B<arc>, or B<archive> (current url or specified)

=cut
$locator_prefixes{'^(archive|arc|ar|ia)(| .*)$'} = 'archive';

=item Freshmeat

B<fm> or B<freshmeat>

=cut
$locator_prefixes{'^(freshmeat|fm)(| .*)$'} = 'freshmeat';

=item SourceForge

B<sf> or B<sourceforge>

=cut
$locator_prefixes{'^(sourceforge|sf)(| .*)$'} = 'sourceforge';

=item Savannah

B<sv> or B<savannah>

=cut
$locator_prefixes{'^(savannah|sv)(| .*)$'} = 'savannah';

=item Gna!

B<gna>

=cut
$locator_prefixes{'^gna(| .*)$'} = 'gna';

=item Netcraft Uptime Survey

B<whatis> or B<uptime> (current url or specified)

=cut

=item Who's Alive and Who's Dead

Wanted, B<dead> or B<alive>!

=cut
$locator_prefixes{'^(alive|dead)(| .*)$'} = 'dead';

=item Google Library / Project Gutenberg

B<book> or B<read>

=cut
$locator_prefixes{'^(book|read)(| .*)$'} = 'book';

=item Internet Public Library

B<ipl>

=cut
$locator_prefixes{'^ipl(| .*)$'} = 'ipl';


#######################################################################

=back

=head2 ELinks

=over 4

=item Home

B<el> or B<elinks>

=item Bugzilla

B<bz> or B<bug> (# or search optional)

=item Documentation and FAQ

B<doc(|s|umentation)> or B<faq>

=back

=cut


my %weather_locators = (
	'weather underground' => 'http://wunderground.com/cgi-bin/findweather/getForecast?query=!query!',
	'google' => 'http://google.com/search?q=weather+"!query!"',
	'yahoo' => 'http://search.yahoo.com/search?p=weather+"!query!"',
	'cnn' => 'http://weather.cnn.com/weather/search?wsearch=!query!',
	'accuweather' => 'http://wwwa.accuweather.com/adcbin/public/us_getcity.asp?zipcode=!query!',
	'ask jeeves' => 'http://web.ask.com/web?&q=weather !query!',
);


sub goto_url_hook
{
	my $url = shift;
	my $current_url = shift;

	# "bugmenot" (no blood today, thank you)
	if ($url eq 'bugmenot' && $current_url)
	{
		($current_url) = $current_url =~ /^.*:\/\/(.*)/;
		return 'http://bugmenot.com/view.php?url=' . $current_url;
	}

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

	# Search engines
	my ($search) = $url =~ /^\S+\s+(.*)/;
	if ($url =~ /^(search|find|www|web|s|f|go)(| .*)$/)
	{
		return search(loadrc('search'), $search);
	}
	if ($url =~ s/^("|\'|')(.+)$/$2/)
	{
		return search(loadrc('search'), $url);
	}
	foreach my $prefix (keys %search_prefixes)
	{
		next unless $url =~ /$prefix/;
		return search($search_prefixes{$prefix}, $search);
	}

	# News
	if ($url =~ /^(news|n)(| .*)$/)
	{
		return news(loadrc('news'), $search);
	}
	foreach my $prefix (keys %news_prefixes)
	{
		next unless $url =~ /$prefix/;
		return news($news_prefixes{$prefix}, $search);
	}

	# Locators
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
		or $url =~ '^(whatis|uptime)(| .*)$') {
		my ($thingy) = $url =~ /^[a-z]* (.*)/;
		my ($domain) = $current_url =~ /([a-z0-9-]+\.(com|net|org|edu|gov|mil))/;

		my $locator_zip            = 'http://usps.com';
		my $ipv                    = "ipv4-address-space"; $ipv = "ipv6-address-space" if loadrc("ipv6") eq "yes";
			my $locator_ip         = 'http://www.iana.org/assignments/' . $ipv;
		my $whois                  = 'http://reports.internic.net/cgi/whois?type=domain&whois_nic=';
			my $locator_whois      = 'http://www.iana.org/cctld/cctld-whois.htm';
			$locator_whois         = $whois . $domain if $domain;
		my $locator_rfc            = 'http://ietf.org';
		my $locator_weather        = 'http://weather.noaa.gov';
		my $locator_whatis         = 'http://uptime.netcraft.com';
			$locator_whatis        = 'http://uptime.netcraft.com/up/graph/?host=' . $domain if $domain;
		if ($thingy)
		{
			$locator_zip           = 'http://zip4.usps.com/zip4/zip_responseA.jsp?zipcode=' . $thingy;
				$locator_zip       = 'http://zipinfo.com/cgi-local/zipsrch.exe?zip=' . $thingy if $thingy !~ '^[0-9]*$';
			$locator_ip            = 'http://melissadata.com/lookups/iplocation.asp?ipaddress=' . $thingy;
			$locator_whois         = $whois . $thingy;
			$locator_rfc           = 'http://rfc-editor.org/cgi-bin/rfcsearch.pl?num=37&searchwords=' . $thingy;
				$locator_rfc       = 'http://ietf.org/rfc/rfc' . $thingy . '.txt' unless $thingy !~ '^[0-9]*$';
			my $weather            = loadrc("weather");
				$locator_weather   = $weather_locators{$weather};
				$locator_weather ||= $weather_locators{'weather underground'};
				$locator_weather   =~ s/!query!/$thingy/;
			$locator_whatis        = 'http://uptime.netcraft.com/up/graph/?host=' . $thingy;
		}
		return $locator_zip         if ($url =~ '^(zip|usps)(| .*)$');
		return $locator_ip          if ($url =~ '^ip(| .*)$');
		return $locator_whois       if ($url =~ '^whois(| .*)$');
		return $locator_rfc         if ($url =~ '^rfc(| .*)$');
		return $locator_weather     if ($url =~ '^(weather|w)(| .*)$');
		return $locator_whatis      if ($url =~ '^(whatis|uptime)(| .*)$');
	}

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

	# The Bastard Operator from Hell
	if ($url =~ '^bofh$')
	{
		return 'http://prime-mover.cc.waikato.ac.nz/Bastard.html';
	}

	# Coral cache <URL>
	if ($url =~ '^(coral|cc|nyud)( .*)$')
	{
		my ($cache) = $url =~ /^[a-z]* (.*)/;
		$cache =~ s/^http:\/\///;
		($url) = $cache =~ s/\//.nyud.net:8090\//;
		return 'http://' . $cache;
	}

	# Babelfish ("babelfish german english"  or  "bf de en")
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

	# XYZZY
	if ($url =~ '^xyzzy$')
	{
		# $url = 'http://sundae.triumf.ca/pub2/cave/node001.html';
		srand();
		my $yzzyx;
		my $xyzzy = int(rand(6));
		$yzzyx = 1   if ($xyzzy == 0); # Colossal Cave Adventure
		$yzzyx = 227 if ($xyzzy == 1); # Zork Zero: The Revenge of Megaboz
		$yzzyx = 3   if ($xyzzy == 2); # Zork I: The Great Underground Empire
		$yzzyx = 4   if ($xyzzy == 3); # Zork II: The Wizard of Frobozz
		$yzzyx = 5   if ($xyzzy == 4); # Zork III: The Dungeon Master
		$yzzyx = 6   if ($xyzzy == 5); # Zork: The Undiscovered Underground
		return 'http://ifiction.org/games/play.php?game=' . $yzzyx;
	}

	# ...and now, Deep Thoughts.  by Jack Handey
	if ($url =~ '^(jack|handey)$')
	{
		return 'http://glug.com/handey';
	}

	# Page validators [<URL>]
	if ($url =~ '^vhtml(| .*)$' or $url =~ '^vcss(| .*)$')
	{
		my ($page) = $url =~ /^.* (.*)/;
		$page = $current_url unless $page;
		return 'http://validator.w3.org/check?uri=' . $page if $url =~ 'html';
		return 'http://jigsaw.w3.org/css-validator/validator?uri=' . $page if $url =~ 'css';
	}

	# There's no place like home
	if ($url =~ '^(el(|inks)|b(ug(|s)|z)(| .*)|doc(|umentation|s)|faq)$')
	{
		my ($bug) = $url =~ /^.* (.*)/;
		if ($url =~ '^b')
		{
			my $bugzilla = 'http://bugzilla.elinks.or.cz';
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
			$doc = '/faq.html' if $url =~ '^faq$';
			return 'http://elinks.or.cz' . $doc;
		}
	}

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


	# Anything not otherwise useful could be a search
	if ($current_url and loadrc("gotosearch") eq "yes")
	{
		return search(loadrc("search"), $url);
	}

	return $url;
}



=head1 GLOBAL URL REWRITES

These rewrites happen everytime ELinks is about to follow an URL and load it,
so this is an order of magnitude more powerful than the Goto URL rewrites.

I<Developer's usage>: The function I<follow_url_hook> is called when the hook
is triggered, taking the target URI as its only argument.  It returns the final
target URI.

These are the default rewrite rules:

=over 4

=cut

sub follow_url_hook
{
	my $url = shift;

=item Google's Bork!

Rewrites many I<google.com> URIs to use the phenomenal I<xx-bork> localization.

=cut
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

Rewrites any I<nntp:>/I<news:> URIs to Google Groups HTTP URIs.

=cut
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

=cut



=head1 HTML REWRITING RULES

When an HTML document is downloaded and is about to undergo the final
rendering, the rewrites described here are done first.  This is frequently
used to get rid of ads, but also various ELinks-unfriendly HTML code and
HTML snippets which are irrelevant to ELinks but can obfuscate the
rendered document.

Note well that these rules are applied B<only> before the final rendering, not
before the gradual re-renderings which happen when only part of the document is
yet available.

I<Developer's usage>: The function I<pre_format_html_hook> is called when the hook
is triggered, taking the document's URI and the HTML source as its two arguments.
It returns the rewritten HTML code.

These are the default rewrite rules:

=over 4

=cut

sub pre_format_html_hook
{
	my $url = shift;
	my $html = shift;

=item Slashdot Sanitation

Kills Slashdot's Advertisements.

I<This rewrite rule is B<DISABLED> now due to certain weird behaviour
it causes.>

=cut
	if ($url =~ 'slashdot\.org')
	{
#		$html =~ s/^<!-- Advertisement code. -->.*<!-- end ad code -->$//sm;
#		$html =~ s/<iframe.*><\/iframe>//g;
#		$html =~ s/<B>Advertisement<\/B>//;
	}

=item Obvious Google Tips Annihilator

Kills some Google tips which are obvious anyway to any ELinks user.

=cut
	if ($url =~ 'google\.com')
	{
		$html =~ s/Teep: In must broosers yuoo cun joost heet zee retoorn key insteed ooff cleecking oon zee seerch boottun\. Bork bork bork!//;
		$html =~ s/Tip:<\/font> Save time by hitting the return key instead of clicking on "search"/<\/font>/;
	}

=item SourceForge AdSmasher

Wipes out SourceForge's Ads.

=cut
	if ($url =~ 'sourceforge\.net')
	{
		$html =~ s/<!-- AD POSITION \d+ -->.*?<!-- END AD POSITION \d+ -->//smg;
		$html =~ s/<b>&nbsp\;&nbsp\;&nbsp\;Site Sponsors<\/b>//g;
	}

=item Gmail's Experience

Gmail has obviously never met ELinks for it to suggest another browser for a better
Gmail experience.

=cut
	if ($url =~ 'gmail\.google\.com')
	{
		$html =~ s/^<b>For a better Gmail experience, use a.+?Learn more<\/a><\/b>$//sm;
	}

=item Source readability improvements

Rewrites some evil characters to entities and vice versa.

=cut
	# TODO: Line wrapping? --pasky
	$html =~ s/Ñ/\&mdash;/g;
	$html =~ s/\&#252/ü/g;
	$html =~ s/\&#039/'/g;

	return $html;
}

=back

=cut



=head1 SMART PROXY USAGE

The Perl hooks are asked whether to use a proxy for a given URI (or what proxy
to actually use). You can use it e.g. if you don't want to use a proxy for
certain Intranet servers but you need to use it in order to get to the
Internet, or if you want to use some anonymizer for access to certain naughty
sites.

I<Developer's usage>: The function I<proxy_for_hook> is called when the hook
is triggered, taking the target URI as its only argument.  It returns the proxy
URI, empty string to use no proxy or I<undef> to use the default proxy URI.

These are the default proxy rules:

=over 4

=cut

sub proxy_for_hook
{
	my $url = shift;

=item No proxy for local files

Prevents proxy usage for local files and C<http://localhost>.

=cut
	if ($url =~ '^(file://|(http://|)(localhost|127\.0\.0\.1)(/|:|$))')
	{
		return "";
	}

	return;
}

=back

=cut



=head1 ON-QUIT ACTIONS

The Perl hooks can also perform various actions when ELinks quits.
These can be things like retouching the just saved "information files",
or doing some fun stuff.

I<Developer's usage>: The function I<quit_hook> is called when the hook
is triggered, taking no arguments nor returning anything.

These are the default actions:

=over 4

=cut

sub quit_hook
{

=item Collapse XBEL Folders

Collapses XBEL bookmark folders.  This is obsoleted by
I<bookmarks.folder_state>.

=cut
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

A few words of wisdom from ELinks the Sage.  (Prints a fortune. ;-)

=cut
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

=cut




=head1 MAPPING ROUTINES

In this section, name mapping routines are described.  They are probably of no
interest to regular hooks users, only for hooks developers.

These routines do a name->URL mapping - e.g. the I<goto_url_hook()> described
above maps a certain prefix to C<google> and then asks the I<search()> mapping
routine described below to map the C<google> string to an appropriate URL.

The mappings themselves should be obvious and are not described here.  Interested
readers can look them up themselves in the I<hooks.pl> file.

There are generally two URLs for each name, one for a direct shortcut jump and
another for a search on the given site (if any string is specified after the
prefix, usually).

=cut


=head2 SEARCH ENGINES

The I<%search_engines> hash maps each engine name to two URLs, I<home> and
I<search>.  In case of I<search>, the query is appended to the mapped URL.

B<!bork!> string in the URL is substitued for an optional I<xx-bork>
localization specifier, but only in the C<google> mapping.

=cut

my %search_engines = (
	"elgoog"     => {
		home     => 'http://alltooflat.com/geeky/elgoog/m/index.cgi',
		search   => 'http://alltooflat.com/geeky/elgoog/m/index.cgi?page=%2fsearch&cgi=get&q='
	},
	"google"     => {
		home     => 'http://google.com!bork!',
		search   => 'http://google.com/search?!bork!q='
	},
	"yahoo"      => {
		home     => 'http://yahoo.com',
		search   => 'http://search.yahoo.com/search?p='
	},
	"ask jeeves" => {
		home     => 'http://ask.com',
		search   => 'http://web.ask.com/web?q='
	},
	"a9"         => {
		home     => 'http://a9.com',
		search   => 'http://a9.com/?q='
	},
	"altavista"  => {
		home     => 'http://altavista.com',
		search   => 'http://altavista.com/web/results?q='
	},
	"msn"        => {
		home     => 'http://msn.com',
		search   => 'http://search.msn.com/results.aspx?q='
	},
	"dmoz"       => {
		home     => 'http://dmoz.org',
		search   => 'http://search.dmoz.org/cgi-bin/search?search='
	},
	"dogpile"    => {
		home     => 'http://dogpile.com',
		search   => 'http://dogpile.com/info.dogpl/search/web/'
	},
	"mamma"      => {
		home     => 'http://mamma.com',
		search   => 'http://mamma.com/Mamma?query='
	},
	"webcrawler" => {
		home     => 'http://webcrawler.com',
		search   => 'http://webcrawler.com/info.wbcrwl/search/web/'
	},
	"netscape"   => {
		home     => 'http://search.netscape.com',
		search   => 'http://channels.netscape.com/ns/search/default.jsp?query='
	},
	"lycos"      => {
		home     => 'http://lycos.com',
		search   => 'http://search.lycos.com/default.asp?query='
	},
	"hotbot"     => {
		home     => 'http://hotbot.com',
		search   => 'http://hotbot.com/default.asp?query='
	},
	"excite"     => {
		home     => 'http://search.excite.com',
		search   => 'http://search.excite.com/info.xcite/search/web/'
	},
);

=pod

The search engines mapping is done by the I<search()> function, taking the
search engine name as its first parameter and optional search string as its
second parameter.  It returns the mapped target URL.

I<Google> is used as the default search engine if the given engine is not
found.

=cut

sub search
{
	my ($engine, $search) = @_;
	my $key = $search ? 'search' : 'home';

	# Google is the default, Google is the best!
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
	$url .= $search if $search;
	return $url;
}


=head2 NEWS SERVERS

The I<%news_servers> hash maps each engine name to two URLs, I<home> and
I<search>.  In case of I<search>, the query is appended to the mapped URL.

=cut

my %news_servers = (
	"bbc"       => {
		home    => 'http://news.bbc.co.uk',
		search  => 'http://newssearch.bbc.co.uk/cgi-bin/search/results.pl?q=',
	},
	# The bastard child of Microsoft and the National Broadcasting Corporation
	"msnbc"     => {
		home    => 'http://msnbc.com',
		search  => 'http://msnbc.msn.com/?id=3053419&action=fulltext&querytext=',
	},
	"cnn"       => {
		home    => 'http://cnn.com',
		search  => 'http://search.cnn.com/pages/search.jsp?query=',
	},
	"fox"       => {
		home    => 'http://foxnews.com',
		search  => 'http://search.foxnews.com/info.foxnws/redirs_all.htm?pgtarg=wbsdogpile&qkw=',
	},
	"google"    => {
		home    => 'http://news.google.com',
		search  => 'http://news.google.com/news?q=',
	},
	"yahoo"     => {
		home    => 'http://news.yahoo.com',
		search  => 'http://news.search.yahoo.com/search/news/?p=',
	},
	"reuters"   => {
		home    => 'http://reuters.com',
		search  => 'http://reuters.com/newsSearchResultsHome.jhtml?query=',
	},
	"eff"       => {
		home    => 'http://eff.org',
		search  => 'http://google.com/search?sitesearch=http://eff.org&q=',
	},
	"wired"     => {
		home    => 'http://wired.com',
		search  => 'http://search.wired.com/wnews/default.asp?query=',
	},
	"slashdot"  => {
		home    => 'http://slashdot.org',
		search  => 'http://slashdot.org/search.pl?query=',
	},
	"newsforge" => {
		home    => 'http://newsforge.com',
		search  => 'http://newsforge.com/search.pl?query=',
	},
	"usnews"    => {
		home    => 'http://usnews.com',
		search  => 'http://www.usnews.com/search/Search?keywords=',
	},
	"newsci"    => {
		home    => 'http://newscientist.com',
		search  => 'http://www.newscientist.com/search.ns?doSearch=true&articleQuery.queryString=',
	},
	"discover"  => {
		home    => 'http://discover.com',
		search  => 'http://www.discover.com/search-results/?searchStr=',
	},
	"sciam"     => {
		home    => 'http://sciam.com',
		search  => 'http://sciam.com/search/index.cfm?QT=Q&SC=Q&Q=',
	},
);

=pod

The news servers mapping is done by the I<news()> function, taking the
search engine name as its first parameter and optional search string as its
second parameter.  It returns the mapped target URL.

I<BBC> is used as the default search engine if the given engine is not
found.

=cut

sub news
{
	my ($server, $search) = @_;
	my $key = $search ? 'search' : 'home';

	# Fall back to the BBC if no preference.
	$server = 'bbc' unless $news_servers{$server}
		and $news_servers{$server}->{$key};
	my $url = $news_servers{$server}->{$key};
	$url .= $search if $search;
	return $url;
}


=head2 LOCATORS

The I<%locators> hash maps each engine name to two URLs, I<home> and
I<search>.

B<!bork!> string in the URL is substitued for an optional I<xx-bork>
localization specifier (for any mappings in this case, not just C<google>).

B<!current!> string in the URL is substitued for the URL of the current
document.

B<!query!> string in the I<search> URL is substitued for the search string.  If
no B<!query!> string is found in the URL, the query is appended to the mapped
URL.

=cut

my %locators = (
	'imdb'        => {
		home      => 'http://imdb.com',
		search    => 'http://imdb.com/Find?select=All&for=',
	},
	'stock'       => {
		home      => 'http://nasdr.com',
		search    => 'http://finance.yahoo.com/l?s=',
	},
	'bs'          => {
		home      => 'http://snopes.com',
		search    => 'http://search.atomz.com/search/?sp-a=00062d45-sp00000000&sp-q=',
	},
	'torrent'     => {
		home      => 'http://isohunt.com',
		search    => 'http://google.com/search?q=filetype:torrent !query!!bork!',
	},
	'archive'     => {
		home      => 'http://web.archive.org/web/*/!current!',
		search    => 'http://web.archive.org/web/*/',
	},
	'freshmeat'   => {
		home      => 'http://freshmeat.net',
		search    => 'http://freshmeat.net/search/?q=',
	},
	'sourceforge' => {
		home      => 'http://sourceforge.net',
		search    => 'http://sourceforge.net/search/?q=',
	},
	'savannah'    => {
		home      => 'http://savannah.nongnu.org',
		search    => 'http://savannah.nongnu.org/search/?type_of_search=soft&words=',
	},
	'gna'         => {
		home      => 'http://gna.org',
		search    => 'https://gna.org/search/?type_of_search=soft&words=',
	},
	'dead'        => {
		home      => 'http://www.whosaliveandwhosdead.com',
		search    => 'http://google.com/search?btnI&sitesearch=http://whosaliveandwhosdead.com&q=',
	},
	'book'        => {
		home      => 'http://gutenberg.org',
		search    => 'http://google.com/search?q=book+"!query!"',
	},
	'ipl'         => {
		home      => 'http://ipl.org',
		search    => 'http://ipl.org/div/searchresults/?words=',
	},
);

=pod

The locators mapping is done by the I<location()> function, taking the search
engine name as its first parameter, optional search string as its second
parameter and the current document's URL as its third parameter.  It returns
the mapped target URL.

An error is produced if the given locator is not found.

=cut

sub location
{
	my ($server, $search, $current_url) = @_;
	my $key = $search ? 'search' : 'home';

	croak 'Unknown URL!' unless $locators{$server}
		and $locators{$server}->{$key};
	my $url = $locators{$server}->{$key};

	my $bork = ""; $bork = "&hl=xx-bork" unless (loadrc("bork") ne "yes");
	$url =~ s/!bork!/$bork/g;

	$url =~ s/!current!/$current_url/g;
	$url .= $search if $search and not $url =~ s/!query!/$search/g;

	return $url;
}


=head1 SEE ALSO

elinks(1), perl(1)


=head1 AUTHORS

Russ Rowan, Petr Baudis

=cut
