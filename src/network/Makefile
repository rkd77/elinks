path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_SSL),yes)
ssldir = ssl
endif

SUBDIRS = $(ssldir)

OBJS = connection.o dns.o progress.o socket.o state.o

include $(path_to_top)/Makefile.lib
