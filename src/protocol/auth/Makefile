path_to_top=../../..
include $(path_to_top)/Makefile.config

# Needed for the digest MD5 sum calculation.
INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

OBJS = auth.o dialogs.o digest.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
