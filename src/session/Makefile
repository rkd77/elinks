path_to_top=../..
include $(path_to_top)/Makefile.config

OBJS = download.o history.o location.o session.o task.o

all-local: libsession.a
libsession.a: $(OBJS)

include $(path_to_top)/Makefile.lib
