# \# ELinks - an advanced web browser

ELinks is an advanced and well-established feature-rich text mode web
(HTTP/FTP/..) browser. ELinks can render both frames and tables, is
highly customizable and can be extended via scripts. It is very portable
and runs on a variety of platforms.

The ELinks official website is available at <http://elinks.cz/>.

Please see the SITES file for mirrors or other recommended sites. If you
want to install ELinks on your computer, see the INSTALL file for
further instructions.

A good starting point is documentation files available in doc/,
especially the file index.txt.

If you want to request features or report bugs, see community
information at <http://elinks.cz/community.html> and feedback
information available at <http://elinks.cz/feedback.html>.

If you want to write some patches, please first read the doc/hacking.txt
document.

If you want to add a new language or update the translation for an
existing one, please read po/README document.

If you want to write some documentation, well, you’re welcome! ;)

# Historical notes

Initially, ELinks was a development version of Links (Lynx-like text WWW
browser), with more liberal features policy and development style. Its
purpose was to provide an alternative to Links, and to test and tune
various new features, but still provide good rock-solid releases inside
stable branches.

Why not contribute to Links instead? Well, first I made a bunch of
patches for the original Links, but Mikulas wasn’t around to integrate
them, so I started releasing my fork. When he came back, a significant
number of them got refused because Mikulas did not like them as he just
wouldn’t have any use for them himself. He aims to keep Links at a
relatively closed feature set and merge only new features which he
himself needs. It has the advantage that the tree is very narrow and the
code is small and contains very little bloat.

ELinks, on the contrary, aims to provide a full-featured web browser,
superior to both lynx and w3m and with the power (but not slowness and
memory usage) of Mozilla, Konqueror and similar browsers. However, to
prevent drastic bloating of the code, the development is driven in the
course of modularization and separation of add-on modules (like cookies,
bookmarks, ssl, scripting etc).

For more details about ELinks history, please see
<http://elinks.cz/history.html>.

If you are more interested in the history and various Links clones and
versions, you can examine the website at <https://web.archive.org/web/20100925114203/http://links.sourceforge.net/>.

Old ELinks team lost interest in ELinks development somehow. felinks
(fork of elinks) is continuation of elinks based on the master branch of
the original elinks repo with main releases (new features and other
incompatible changes) no more often than 1 yearly, and point releases
(bugfixes) no more often than once a month.

Repository was renamed to elinks on 2020-12-01 with Petr’s approval.

Main repo is at <https://github.com/rkd77/elinks.git>.

vim: textwidth=80
