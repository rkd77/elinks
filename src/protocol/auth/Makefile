top_builddir=../../..
include $(top_builddir)/Makefile.config

# Needed for the digest MD5 sum calculation.
INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

OBJS = auth.o dialogs.o digest.o

include $(top_srcdir)/Makefile.lib
