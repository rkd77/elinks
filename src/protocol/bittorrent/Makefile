top_builddir=../../..
include $(top_builddir)/Makefile.config

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

OBJS = \
 bencoding.o \
 bittorrent.o \
 common.o \
 connection.o \
 dialogs.o \
 peerconnect.o \
 peerwire.o \
 piececache.o \
 tracker.o

include $(top_srcdir)/Makefile.lib
