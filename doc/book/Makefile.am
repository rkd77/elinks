## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.13 2004/08/15 05:51:13 miciah Exp $

include $(top_srcdir)/doc/book/Makefile.doc

SUBDIRS = manpages

EXTRA_PROGRAMS = book
book_SOURCES = book.xml
book_HTMLFORMAT = book.html

DISTCLEANFILES = config/cmdoptions.xml

EXTRA_DIST = \
 $(book_SOURCES) \
 Makefile.doc \
 README \
 TODO \
 book.xml \
 elinksbook.dtd \
 config/elinks.conf.xml \
 config/envvars.xml \
 config/examples.xml \
 config/files.xml \
 config/mime.xml \
 config/syntax.xml \
 features/bookmarks.xml \
 info/authors.xml \
 info/preface.xml \
 info/revision.xml \
 intro/installation.xml
