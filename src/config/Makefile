-include ../../Makefile.config

OBJS = cmdline.o conf.o dialogs.o home.o kbdbind.o options.o opttypes.o timer.o urlhist.o

all: libconfig.a
libconfig.a: $(OBJS)

clean:
	rm -f *.o *.a

-include ../../Makefile.lib
