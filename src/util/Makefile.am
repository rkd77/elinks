## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.53 2005/08/17 15:26:22 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_DEBUG
debugobj = memdebug.o
endif

if CONFIG_MD5
md5obj = md5.o
endif

if CONFIG_SCANNER
scannerobj = scanner.o
endif

if CONFIG_SHA1
sha1obj = sha1.o
endif

if CONFIG_SMALL
else
fastfindobj = fastfind.o
endif

noinst_LIBRARIES = libutil.a

EXTRA_libutil_a_SOURCES = \
 fastfind.c \
 fastfind.h \
 md5.c \
 md5.h \
 memdebug.c \
 memdebug.h \
 scanner.c \
 scanner.h \
 sha1.c \
 sha1.h

libutil_a_LIBADD = \
 $(debugobj) \
 $(fastfindobj) \
 $(md5obj) \
 $(scannerobj) \
 $(sha1obj)

libutil_a_SOURCES = \
 align.h \
 base64.c \
 base64.h \
 bitfield.h \
 box.h	\
 color.c \
 color.h \
 color.inc \
 color_s.inc \
 conv.c \
 conv.h \
 env.c \
 env.h \
 error.c \
 error.h \
 file.c \
 file.h \
 hash.c \
 hash.h \
 lists.h \
 math.h \
 memlist.c \
 memlist.h \
 memory.c \
 memory.h \
 profile.h \
 secsave.c \
 secsave.h \
 snprintf.c \
 snprintf.h \
 string.c \
 string.h \
 time.c \
 time.h
