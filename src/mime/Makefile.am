## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4 2004/05/02 13:08:17 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_MAILCAP
mailcapobj = backend/mailcap.o
endif

if CONFIG_MIMETYPES
mimetypesobj = backend/mimetypes.o
endif

SUBDIRS = backend

noinst_LIBRARIES = libmime.a

libmime_a_LIBADD = \
 backend/common.o \
 backend/default.o \
 $(mailcapobj) \
 $(mimetypesobj)

libmime_a_SOURCES = \
 dialogs.c \
 dialogs.h \
 mime.c \
 mime.h
