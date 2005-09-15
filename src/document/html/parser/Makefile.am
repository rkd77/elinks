## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.6 2004/08/14 21:12:39 miciah Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/document/ at all, and we can't
# merge the whole library of these with libdocument.a. So we only compile
# objects here and link them directly to libdocument.a in ../Makefile.am.

# However, we apparently need to create the library in order to convince
# automake to compile the objects :/. We DON'T use libhtmlp.a ANYWHERE.

noinst_LIBRARIES = libhtmlp.a
libhtmlp_a_SOURCES = \
 forms.c \
 forms.h \
 link.c \
 link.h \
 parse.c \
 parse.h \
 stack.c \
 stack.h \
 table.c \
 table.h

