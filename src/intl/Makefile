path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_NLS),yes)
gettextdir = gettext
endif

SUBDIRS = $(gettextdir)

OBJS = charsets.o

all-local: libintl.a
libintl.a: $(OBJS)

include $(path_to_top)/Makefile.lib
