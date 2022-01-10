top_builddir=../..
include $(top_builddir)/Makefile.config

INCLUDES += $(GNUTLS_CFLAGS) $(OPENSSL_CFLAGS)

SUBDIRS = qs_parse

OBJS-unless$(CONFIG_SMALL)		 += fastfind.o
OBJS-$(CONFIG_CSS)			 += scanner.o
OBJS-$(CONFIG_DEBUG)			 += memdebug.o
OBJS-$(CONFIG_DOM)			 += scanner.o

ifeq ($(CONFIG_BITTORRENT),yes)
OBJS-$(CONFIG_BITTORRENT)		+= scanner.o
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
 md5.o \
 memlist.o \
 memory.o \
 random.o \
 secsave.o \
 snprintf.o \
 string.o \
 time.o

include $(top_srcdir)/Makefile.lib
