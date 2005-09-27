path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_CSS),yes)
cssdir = css
cssobj = $(cssdir)/lib.o
endif

ifeq ($(CONFIG_DOM),yes)
domdir = dom
domobj = $(domdir)/lib.o

sgmldir = sgml
sgmlobj = $(sgmldir)/lib.o
endif

SUBDIRS = $(cssdir) $(domdir) html plain $(sgmldir)

SUB_OBJS = \
 $(cssobj) \
 $(domobj) \
 html/lib.o \
 plain/lib.o \
 $(sgmlobj)

OBJS = docdata.o document.o forms.o options.o refresh.o renderer.o $(SUB_OBJS)

include $(path_to_top)/Makefile.lib
