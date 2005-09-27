path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_DEBUG),yes)
debugobj = memdebug.o
endif

ifeq ($(CONFIG_MD5),yes)
md5obj = md5.o
endif

ifeq ($(CONFIG_SCANNER),yes)
scannerobj = scanner.o
endif

ifeq ($(CONFIG_SHA1),yes)
sha1obj = sha1.o
endif

ifeq ($(CONFIG_SMALL),yes)
else
fastfindobj = fastfind.o
endif

ifeq ($(CONFIG_MOUSE),yes)
mouseobj = mouse.o
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
 secsave.o \
 snprintf.o \
 string.o \
 time.o \
 $(debugobj) \
 $(fastfindobj) \
 $(md5obj) \
 $(scannerobj) \
 $(sha1obj)

all-local: libutil.a
libutil.a: $(OBJS)

include $(path_to_top)/Makefile.lib
