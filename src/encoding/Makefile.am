## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.2 2004/05/28 11:55:26 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_BZIP2
bzip2obj	= bzip2.o
endif

if CONFIG_GZIP
gzipobj		= gzip.o
endif

noinst_LIBRARIES = libencoding.a

EXTRA_libencoding_a_SOURCES = \
 bzip2.c \
 bzip2.h \
 gzip.c \
 gzip.h

libencoding_a_LIBADD = $(bzip2obj) $(gzipobj)

libencoding_a_SOURCES = \
 encoding.c \
 encoding.h

