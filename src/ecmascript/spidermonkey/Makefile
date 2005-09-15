-include ../../../Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
OBJS = document.o form.o location.o navigator.o unibar.o window.o

all: $(OBJS)

clean:
	rm -f *.o *.a

-include ../../../Makefile.lib
