path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_NLS),yes)
gettextdir = gettext
endif

SUBDIRS = $(gettextdir)

OBJS = charsets.o

include $(path_to_top)/Makefile.lib
