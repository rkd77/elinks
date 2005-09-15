## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.7 2005/06/12 02:26:49 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = TODO

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

noinst_LIBRARIES = libssl.a
libssl_a_SOURCES = \
 socket.c \
 socket.h \
 ssl.c \
 ssl.h

