## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.5 2005/04/01 17:36:46 zas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = README

INCLUDES += $(GUILE_CFLAGS)

noinst_LIBRARIES = libscriptingguile.a
libscriptingguile_a_SOURCES = \
 core.c \
 core.h \
 hooks.c \
 hooks.h \
 guile.c \
 guile.h
