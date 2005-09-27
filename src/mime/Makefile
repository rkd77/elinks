path_to_top=../..
include $(path_to_top)/Makefile.config

SUBDIRS = backend

ifeq ($(CONFIG_MAILCAP),yes)
mailcapobj = backend/mailcap.o
endif

ifeq ($(CONFIG_MIMETYPES),yes)
mimetypesobj = backend/mimetypes.o
endif

OBJS = \
 dialogs.o \
 mime.o \
 backend/common.o \
 backend/default.o \
 $(mailcapobj) \
 $(mimetypesobj)

all-local: libmime.a
libmime.a: $(OBJS)

include $(path_to_top)/Makefile.lib
