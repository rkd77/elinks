top_builddir=../../..
include $(top_builddir)/Makefile.config

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS) $(LIBGCRYPT_CFLAGS)

SUBDIRS = test

# ELinks uses match-hostname.o only if CONFIG_OPENSSL.
# However, match-hostname.o has test cases that always need it.
# The test framework doesn't seem to support conditional tests.
OBJS = match-hostname.o ssl.o socket.o

include $(top_srcdir)/Makefile.lib
