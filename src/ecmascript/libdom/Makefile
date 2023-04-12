top_builddir=../../..
include $(top_builddir)/Makefile.config
INCLUDES += $(LIBDOM_CFLAGS)

SUBDIRS-$(CONFIG_MUJS)		+= mujs
SUBDIRS-$(CONFIG_QUICKJS)	+= quickjs

OBJS = parse.o

include $(top_srcdir)/Makefile.lib
