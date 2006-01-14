top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

SUBDIRS-$(CONFIG_ECMASCRIPT_SEE)	+= see
SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS)	+= spidermonkey

OBJS-$(CONFIG_ECMASCRIPT_SEE)		+= see.o
OBJS-$(CONFIG_ECMASCRIPT_SMJS)		+= spidermonkey.o

OBJS	= ecmascript.o

include $(top_srcdir)/Makefile.lib
