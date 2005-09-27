path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_MOUSE),yes)
mouseobj = mouse.o
endif

OBJS = \
 color.o \
 draw.o \
 event.o \
 hardio.o \
 kbd.o \
 $(mouseobj) \
 screen.o \
 tab.o \
 terminal.o \
 window.o

include $(path_to_top)/Makefile.lib
