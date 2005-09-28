path_to_top=../..
include $(path_to_top)/Makefile.config

# Reverse a CONFIG_* string
# Usage $(call not,$(CONFIG_FOO))
not = $(if $(findstring yes,$(1)),no,yes)

OBJS-$(CONFIG_DEBUG)		 += memdebug.o
OBJS-$(CONFIG_MD5)		 += md5.o
OBJS-$(CONFIG_SCANNER)		 += scanner.o
OBJS-$(CONFIG_SHA1)		 += sha1.o
OBJS-$(call not,$(CONFIG_SMALL)) += fastfind.o

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
 secsave.o \
 snprintf.o \
 string.o \
 time.o

include $(path_to_top)/Makefile.lib
