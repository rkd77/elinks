path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_SSL),yes)
ssldir = ssl
sslobj = $(ssldir)/ssl.o $(ssldir)/socket.o
endif

SUBDIRS = $(ssldir)

OBJS = connection.o dns.o progress.o socket.o state.o $(sslobj)

all-local: libnetwork.a
libnetwork.a: $(OBJS)

include $(path_to_top)/Makefile.lib
