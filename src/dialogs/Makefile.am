## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.26 2005/04/18 17:00:25 zas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_EXMODE
exmodeobj = exmode.o
endif

noinst_LIBRARIES = libdialogs.a

EXTRA_libdialogs_a_SOURCES = exmode.c exmode.h
libdialogs_a_LIBADD = $(exmodeobj)

libdialogs_a_SOURCES = \
 document.c \
 document.h \
 download.c \
 download.h \
 edit.c \
 edit.h \
 info.c \
 info.h \
 menu.c \
 menu.h \
 options.c \
 options.h \
 progress.c \
 progress.h \
 status.c \
 status.h
