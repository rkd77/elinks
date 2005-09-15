## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.13 2005/01/05 14:37:44 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_NLS
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
