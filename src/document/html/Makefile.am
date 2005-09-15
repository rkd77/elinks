## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.18 2004/08/14 21:12:38 miciah Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/document/ at all, and we can't
# merge the whole library of these with libdocument.a. So we only compile
# objects here and link them directly to libdocument.a in ../Makefile.am.

# However, we apparently need to create the library in order to convince
# automake to compile the objects :/. We DON'T use libhtml.a ANYWHERE.

EXTRA_DIST = TODO

SUBDIRS = parser

noinst_LIBRARIES = libhtml.a
libhtml_a_SOURCES = \
 frames.c \
 frames.h \
 internal.h \
 parser.c \
 parser.h \
 renderer.c \
 renderer.h \
 tables.c \
 tables.h

