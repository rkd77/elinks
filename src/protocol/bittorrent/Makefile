path_to_top=../../..
include $(path_to_top)/Makefile.config

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

OBJS = bencoding.o bittorrent.o common.o connection.o dialogs.o peerconnect.o peerwire.o piececache.o tracker.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
