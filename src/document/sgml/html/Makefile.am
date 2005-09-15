## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2004/09/24 00:44:59 jonas Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/document/ at all, and we can't
# merge the whole library of these with libdocument.a. So we only compile
# objects here and link them directly to libdocument.a in ../Makefile.am.

# However, we apparently need to create the library in order to convience
# automake to compile the objects :/. We DON'T use libcss.a ANYWHERE.

noinst_LIBRARIES = libhtml.a
libhtml_a_SOURCES = \
 attribute.inc \
 element.inc \
 html.c \
 html.h
