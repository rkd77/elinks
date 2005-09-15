## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.5 2004/08/14 21:12:39 miciah Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of src/mime/ at all, and we can't merge
# the whole library of these with libmime.a. So we only compile objects here
# and link them directly to libmime.a in ../Makefile.am.

# However, we apparently need to create the library in order to convince
# automake to compile the objects :/. We DON'T use libmimebackend.a ANYWHERE.

if CONFIG_MAILCAP
mailcapobj = mailcap.o
endif

if CONFIG_MIMETYPES
mimetypesobj = mimetypes.o
endif

noinst_LIBRARIES = libmimebackend.a

EXTRA_libmimebackend_a_SOURCES = \
 mailcap.c \
 mailcap.h \
 mimetypes.c \
 mimetypes.h

libmimebackend_a_LIBADD = $(mailcapobj) $(mimetypesobj)

libmimebackend_a_SOURCES = \
 common.c \
 common.h \
 default.c \
 default.h
