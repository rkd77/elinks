## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2004/05/21 12:03:52 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_CGI
cgiobj = cgi.o
endif

noinst_LIBRARIES = libfile.a

EXTRA_libfile_a_SOURCES = cgi.c cgi.h
libfile_a_LIBADD = $(cgiobj)

libfile_a_SOURCES = \
 file.c \
 file.h

