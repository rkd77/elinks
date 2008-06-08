top_builddir=../..
include $(top_builddir)/Makefile.config

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

OBJS-unless$(CONFIG_SMALL)		 += fastfind.o
OBJS-$(CONFIG_CSS)			 += scanner.o
OBJS-$(CONFIG_DEBUG)			 += memdebug.o
OBJS-$(CONFIG_DOM)			 += scanner.o

# Use own MD5 implementation if testing libc or there's no OpenSSL
# (either the real thing or GNUTLS compat wrappers).
OBJS-$(CONFIG_OWN_LIBC)			 += md5.o
ifeq ($(CONFIG_GNUTLS_OPENSSL_COMPAT),no)
OBJS-unless$(CONFIG_OPENSSL)		 += md5.o
endif

ifeq ($(CONFIG_BITTORRENT),yes)
# BitTorrent is the only user. Compile in SHA1 if testing libc or
# there is no OpenSSL. The GNUTLS OpenSSL wrappers doesn't have it.
OBJS-$(CONFIG_OWN_LIBC)			 += sha1.o
OBJS-unless$(CONFIG_OPENSSL)		 += sha1.o
endif

OBJS = \
 base64.o \
 color.o \
 conv.o \
 env.o \
 error.o \
 file.o \
 hash.o \
 memlist.o \
 memory.o \
 random.o \
 secsave.o \
 snprintf.o \
 string.o \
 time.o

include $(top_srcdir)/Makefile.lib
