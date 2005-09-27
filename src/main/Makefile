path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_INTERLINK),yes)
interlinkobj = interlink.o
endif

OBJS = event.o $(interlinkobj) main.o module.o select.o timer.o version.o

include $(path_to_top)/Makefile.lib
