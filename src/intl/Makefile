top_builddir=../..
include $(top_builddir)/Makefile.config

ifeq ($(CONFIG_GETTEXT), no)
SUBDIRS-$(CONFIG_NLS) += gettext
endif

OBJS = charsets.o

OBJS-$(CONFIG_GETTEXT) += libintl.o
OBJS-$(CONFIG_UTF8) += width.o

include $(top_srcdir)/Makefile.lib
