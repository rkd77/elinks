path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_EXMODE),yes)
exmodeobj = exmode.o
endif

OBJS = document.o download.o edit.o $(exmodeobj) info.o menu.o options.o progress.o status.o

all-local: libdialogs.a
libdialogs.a: $(OBJS)

include $(path_to_top)/Makefile.lib
