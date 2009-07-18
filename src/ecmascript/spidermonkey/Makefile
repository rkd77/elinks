top_builddir=../../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

OBJS-$(CONFIG_ECMASCRIPT_SMJS_HEARTBEAT) += heartbeat.o

OBJS = document.o form.o location.o navigator.o unibar.o window.o

include $(top_srcdir)/Makefile.lib
