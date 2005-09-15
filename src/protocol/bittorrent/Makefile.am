## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.3 2005/07/21 08:23:07 zas Exp $

include $(top_srcdir)/Makefile.base

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

noinst_LIBRARIES = libbittorrent.a

libbittorrent_a_SOURCES = \
 bencoding.c \
 bencoding.h \
 bittorrent.c \
 bittorrent.h \
 common.c \
 common.h \
 connection.c \
 connection.h \
 dialogs.c \
 dialogs.h \
 peerconnect.c \
 peerconnect.h \
 peerwire.c \
 peerwire.h \
 piececache.c \
 piececache.h \
 tracker.c \
 tracker.h
