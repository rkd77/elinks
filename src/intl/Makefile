path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_NLS),yes)
gettextdir = gettext
gettextobj = $(gettextdir)/lib.o
endif

SUBDIRS = $(gettextdir)

OBJS = charsets.o $(gettextobj)

include $(path_to_top)/Makefile.lib
