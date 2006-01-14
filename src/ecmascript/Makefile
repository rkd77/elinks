top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS) += spidermonkey
SUBDIRS-$(CONFIG_SEE) += see
OBJS-$(CONFIG_ECMASCRIPT_SMJS) += spidermonkey.o
OBJS-$(CONFIG_SEE) += see.o
OBJS	= ecmascript.o

include $(top_srcdir)/Makefile.lib
