top_builddir=../..
include $(top_builddir)/Makefile.config

OBJS-$(CONFIG_BZIP2)	+= bzip2.o
OBJS-$(CONFIG_GZIP)	+= deflate.o
OBJS-$(CONFIG_LZMA)	+= lzma.o LzmaDecode.o

OBJS = encoding.o

include $(top_srcdir)/Makefile.lib
