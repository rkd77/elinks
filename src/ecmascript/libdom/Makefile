top_builddir=../../..
include $(top_builddir)/Makefile.config
INCLUDES += $(LIBDOM_CFLAGS)

SUBDIRS-$(CONFIG_QUICKJS)	+= quickjs
SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS)	+= spidermonkey

OBJS = parse.o

include $(top_srcdir)/Makefile.lib
