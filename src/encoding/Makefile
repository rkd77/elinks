path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_BZIP2),yes)
bzip2obj	= bzip2.o
endif

ifeq ($(CONFIG_GZIP),yes)
gzipobj		= gzip.o
endif

OBJS = encoding.o $(bzip2obj) $(gzipobj)

include $(path_to_top)/Makefile.lib
