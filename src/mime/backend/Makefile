path_to_top=../../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_MAILCAP),yes)
mailcapobj = mailcap.o
endif

ifeq ($(CONFIG_MIMETYPES),yes)
mimetypesobj = mimetypes.o
endif

OBJS = common.o default.o $(mailcapobj) $(mimetypesobj)

include $(path_to_top)/Makefile.lib
