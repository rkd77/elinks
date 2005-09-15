## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.7 2005/01/13 15:55:57 jonas Exp $

include $(top_srcdir)/Makefile.base

# Needed for the digest MD5 sum calculation.
INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

noinst_LIBRARIES = libauth.a
libauth_a_SOURCES = \
 auth.h \
 auth.c \
 dialogs.h \
 dialogs.c \
 digest.c \
 digest.h
