path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_CSS),yes)
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

ifeq ($(CONFIG_DOM),yes)
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

SUB_OBJS = \
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

OBJS = docdata.o document.o forms.o options.o refresh.o renderer.o $(SUB_OBJS)

all-l: libdocument.a
libdocument.a: $(OBJS)

include $(path_to_top)/Makefile.lib
