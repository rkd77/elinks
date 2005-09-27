path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_SSL),yes)
ssldir = ssl
sslobj = $(ssldir)/lib.o
endif

SUBDIRS = $(ssldir)

OBJS = connection.o dns.o progress.o socket.o state.o $(sslobj)

include $(path_to_top)/Makefile.lib
