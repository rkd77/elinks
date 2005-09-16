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

all-l: libterminal.a
libterminal.a: $(OBJS)

include $(path_to_top)/Makefile.lib
