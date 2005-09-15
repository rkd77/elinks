## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.12 2004/05/02 12:59:42 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_XBEL_BOOKMARKS
xbelobj = backend/xbel.o
endif

SUBDIRS = backend

noinst_LIBRARIES = libbookmarks.a

libbookmarks_a_LIBADD = \
 backend/common.o \
 backend/default.o \
 $(xbelobj)

libbookmarks_a_SOURCES = \
 bookmarks.c \
 bookmarks.h \
 dialogs.c \
 dialogs.h
