path_to_top=../..
include $(path_to_top)/Makefile.config

SUBDIRS = backend

ifeq ($(CONFIG_XBEL_BOOKMARKS),yes)
xbelobj = xbel.o
endif

BACK_OBJS = common.o default.o $(xbelobj)
OBJS = bookmarks.o dialogs.o $(foreach obj,$(BACK_OBJS),backend/$(obj))

all-l: libbookmarks.a
libbookmarks.a: $(OBJS)

include $(path_to_top)/Makefile.lib
