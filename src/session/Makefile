-include ../../Makefile.config

OBJS = download.o history.o location.o session.o task.o

all: libsession.a
libsession.a: $(OBJS)

clean:
	rm -f *.o *.a

-include ../../Makefile.lib
