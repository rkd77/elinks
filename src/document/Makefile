top_builddir=../..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_CSS)	+= css
SUBDIRS-$(CONFIG_LIBCSS)	+= css2
SUBDIRS-$(CONFIG_DOM)	+= dom
SUBDIRS-$(CONFIG_XML)	+= xml

SUBDIRS = gemini html plain

OBJS = docdata.o document.obj format.o forms.o options.o refresh.o renderer.obj

include $(top_srcdir)/Makefile.lib
