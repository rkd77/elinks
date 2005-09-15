## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.13 2004/01/03 01:11:57 pasky Exp $

include $(top_srcdir)/Makefile.base

noinst_LIBRARIES = libcookies.a
libcookies_a_SOURCES = \
 cookies.c \
 cookies.h \
 dialogs.c \
 dialogs.h \
 parser.c \
 parser.h

TESTS = parsetst

EXTRA_PROGRAMS = parsetst

parsetst_SOURCES = \
 parser.h \
 parser.c \
 parsetst.c
parsetst_DEPENDENCIES = testutils
parsetst_LDADD = $(top_builddir)/src/util/libutil.a
