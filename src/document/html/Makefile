top_builddir=../../..
include $(top_builddir)/Makefile.config

# The original HTML parser
SUBDIRS-unless$(CONFIG_DOM_HTML) = mikuparser
OBJS-unless$(CONFIG_DOM_HTML)	 = tables.o

# The DOM-based HTML parser
SUBDIRS-$(CONFIG_DOM_HTML) = domparser

OBJS	= frames.o parser.o renderer.o

include $(top_srcdir)/Makefile.lib
