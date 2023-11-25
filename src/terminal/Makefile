top_builddir=../..
include $(top_builddir)/Makefile.config

OBJS-$(CONFIG_MOUSE) += mouse.o
OBJS-$(CONFIG_LIBSIXEL) += sixel.o
OBJS-$(CONFIG_TERMINFO) += terminfo.o

OBJS = \
 color.o \
 draw.o \
 event.o \
 hardio.o \
 kbd.o \
 screen.o \
 tab.o \
 terminal.o \
 window.o

include $(top_srcdir)/Makefile.lib
