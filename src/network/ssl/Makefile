path_to_top=../../..
include $(path_to_top)/Makefile.config

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

OBJS = ssl.o socket.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
