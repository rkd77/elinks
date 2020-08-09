top_builddir=../..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_NLS) += gettext

OBJS = charsets.o

OBJS-$(CONFIG_UTF8) += width.o

include $(top_srcdir)/Makefile.lib
