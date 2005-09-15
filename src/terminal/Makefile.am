## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.16 2005/09/14 12:50:15 zas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_MOUSE
mouseobj = mouse.o
endif

EXTRA_DIST = TODO

noinst_LIBRARIES = libterminal.a

EXTRA_libterminal_a_SOURCES = \
 mouse.c
libterminal_a_LIBADD = \
 $(mouseobj)

libterminal_a_SOURCES = \
 color.c \
 color.h \
 draw.c \
 draw.h \
 event.c \
 event.h \
 hardio.c \
 hardio.h \
 itrm.h \
 kbd.c \
 kbd.h \
 key.inc \
 mouse.h \
 palette.inc \
 screen.c \
 screen.h \
 tab.c \
 tab.h \
 terminal.c \
 terminal.h \
 window.c \
 window.h
