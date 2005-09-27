path_to_top=../..
include $(path_to_top)/Makefile.config

SUBDIRS = dump text

ifeq ($(CONFIG_MARKS),yes)
marksobj = text/marks.o
endif

OBJS = \
 action.o \
 timer.o \
 dump/dump.o \
 text/draw.o \
 text/form.o \
 text/link.o \
 $(marksobj) \
 text/search.o \
 text/textarea.o \
 text/view.o \
 text/vs.o

all-local: libviewer.a
libviewer.a: $(OBJS)

include $(path_to_top)/Makefile.lib
