## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.12 2004/07/17 20:08:54 pasky Exp $

include $(top_srcdir)/Makefile.base

if ENABLE_NLS
gettextdir = gettext
endif

SUBDIRS = $(gettextdir)

noinst_LIBRARIES = libintl.a
libintl_a_SOURCES = \
 charsets.c \
 charsets.h \
 codepage.inc \
 entity.inc \
 uni_7b.inc
