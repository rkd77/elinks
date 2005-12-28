top_builddir=../..
include $(top_builddir)/Makefile.config

SUBDIRS = css sgml
OBJS = node.o select.o stack.o scanner.o

SUBDIRS-$(CONFIG_DEBUG) += test

include $(top_srcdir)/Makefile.lib
