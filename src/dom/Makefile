top_builddir=../..
include $(top_builddir)/Makefile.config

INCLUDES += $(SPIDERMONKEY_CFLAGS)
SUBDIRS = css sgml test
OBJS = configuration.o node.o select.o stack.o scanner.o

SUBDIRS-$(CONFIG_DEBUG) += test

include $(top_srcdir)/Makefile.lib
