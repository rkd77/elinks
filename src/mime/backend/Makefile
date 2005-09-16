path_to_top=../../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_MAILCAP),yes)
mailcapobj = mailcap.o
endif

ifeq ($(CONFIG_MIMETYPES),yes)
mimetypesobj = mimetypes.o
endif

OBJS = common.o default.o $(mailcapobj) $(mimetypesobj)

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
