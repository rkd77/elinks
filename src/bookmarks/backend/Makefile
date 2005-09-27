path_to_top=../../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_XBEL_BOOKMARKS),yes)
xbelobj = xbel.o
endif

OBJS = common.o default.o $(xbelobj)

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-local: $(OBJS)

include $(path_to_top)/Makefile.lib
