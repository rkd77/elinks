top_builddir=../..
include $(top_builddir)/Makefile.config

INCLUDES += $(LIBDOM_CFLAGS)

SUBDIRS-$(CONFIG_CSS)	+= css
SUBDIRS-$(CONFIG_DOM)	+= dom
SUBDIRS-$(CONFIG_LIBDOM) += libdom

SUBDIRS = html plain

OBJS = docdata.o document.o format.o forms.o options.o refresh.o renderer.o

include $(top_srcdir)/Makefile.lib
