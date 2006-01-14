top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

SUBDIRS-$(CONFIG_SPIDERMONKEY) += spidermonkey
SUBDIRS-$(CONFIG_SEE) += see
OBJS-$(CONFIG_SPIDERMONKEY) += spidermonkey.o
OBJS-$(CONFIG_SEE) += see.o
OBJS	= ecmascript.o

include $(top_srcdir)/Makefile.lib
