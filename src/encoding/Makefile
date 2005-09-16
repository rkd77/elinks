path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_BZIP2),yes)
bzip2obj	= bzip2.o
endif

ifeq ($(CONFIG_GZIP),yes)
gzipobj		= gzip.o
endif

OBJS = encoding.o bzip2.o gzip.o

all-l: libencoding.o
libencoding.a: $(OBJS)

include $(path_to_top)/Makefile.lib
