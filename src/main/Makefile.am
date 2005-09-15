## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.27 2005/08/17 15:26:22 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_INTERLINK
interlinkobj = interlink.o
endif

EXTRA_libmain_a_SOURCES = \
 interlink.c \
 interlink.h

libmain_a_LIBADD = \
 $(interlinkobj)

noinst_LIBRARIES = libmain.a
libmain_a_SOURCES = \
 event.c \
 event.h \
 main.c \
 main.h \
 module.c \
 module.h \
 object.h \
 select.c \
 select.h \
 timer.c \
 timer.h \
 version.c \
 version.h

