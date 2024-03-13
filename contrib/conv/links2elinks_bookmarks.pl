#!/usr/bin/perl

# Usage example:
# ./links2elinks_bookmarks.pl < ~/.links2/bookmarks.html >> ~/.config/elinks/bookmarks

use 5.036;
while (<>) {
	state $d = 0;
	$d++ if m,^<DL>$,;
	$d-- if m,^</DL>$,;
	say "$1\t\t$d\tF" if m,<DT><H3>(.*)</H3>,;
	say "$2\t$1\t$d\t" if m,<DT><A HREF="(.*)">(.*)</A>,;
}
