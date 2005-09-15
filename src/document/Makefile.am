## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.49 2005/07/15 04:37:55 miciah Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_CSS
cssdir = css
cssobj = \
 css/apply.o \
 css/css.o \
 css/parser.o \
 css/property.o \
 css/scanner.o \
 css/stylesheet.o \
 css/value.o
endif

if CONFIG_DOM
domdir = dom
domobj = \
 $(domdir)/navigator.o \
 $(domdir)/node.o \
 $(domdir)/renderer.o

sgmldir = sgml
sgmlobj = \
 $(sgmldir)/html/html.o \
 $(sgmldir)/parser.o \
 $(sgmldir)/scanner.o \
 $(sgmldir)/sgml.o
endif

SUBDIRS = $(cssdir) $(domdir) html plain $(sgmldir)

noinst_LIBRARIES = libdocument.a

libdocument_a_LIBADD = \
 $(cssobj) \
 $(domobj) \
 html/parser/forms.o \
 html/parser/general.o \
 html/parser/link.o \
 html/parser/parse.o \
 html/parser/stack.o \
 html/parser/table.o \
 html/frames.o \
 html/parser.o \
 html/renderer.o \
 html/tables.o \
 plain/renderer.o \
 $(sgmlobj)

libdocument_a_SOURCES = \
 docdata.c \
 docdata.h \
 document.c \
 document.h \
 forms.c \
 forms.h \
 options.c \
 options.h \
 refresh.c \
 refresh.h \
 renderer.c \
 renderer.h \
 view.h

