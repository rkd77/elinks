## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.15 2005/04/01 09:41:37 zas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = TODO

INCLUDES += $(LUA_CFLAGS)

noinst_LIBRARIES = libscriptinglua.a
libscriptinglua_a_SOURCES = \
 core.c \
 core.h \
 hooks.c \
 hooks.h \
 lua.c \
 lua.h

