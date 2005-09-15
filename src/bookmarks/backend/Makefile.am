## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2004/08/14 21:12:36 miciah Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/bookmarks/ at all, and we can't
# merge the whole library of these with libbookmarks.a. So we only compile
# objects here and link them directly to libbookmarks.a in ../Makefile.am.

# However, we apparently need to create the library in order to convince
# automake to compile the objects :/. We DON'T use libbackend.a ANYWHERE.

if CONFIG_XBEL_BOOKMARKS
xbelobj = xbel.o
endif

noinst_LIBRARIES = libbackend.a

EXTRA_libbackend_a_SOURCES = xbel.c xbel.h
libbackend_a_LIBADD = $(xbelobj)

libbackend_a_SOURCES = \
 common.c \
 common.h \
 default.c \
 default.h
