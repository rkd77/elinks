path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_CSS),yes)
cssdir = css
endif

ifeq ($(CONFIG_DOM),yes)
domdir = dom
sgmldir = sgml
endif

SUBDIRS = $(cssdir) $(domdir) html plain $(sgmldir)

OBJS = docdata.o document.o forms.o options.o refresh.o renderer.o

include $(path_to_top)/Makefile.lib
