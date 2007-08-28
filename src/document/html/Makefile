top_builddir=../../..
include $(top_builddir)/Makefile.config

# The original HTML parser
SUBDIRS-unless$(CONFIG_DOM_HTML) = parser
OBJS-unless$(CONFIG_DOM_HTML)	 = parser.o tables.o

# The DOM-based HTML parser
OBJS-$(CONFIG_DOM_HTML) = dom.o

OBJS	= frames.o renderer.o

include $(top_srcdir)/Makefile.lib
