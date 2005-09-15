-include ../../Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

SUBDIRS = spidermonkey
SM_OBJS = document.o form.o location.o navigator.o unibar.o window.o
OBJS = ecmascript.o spidermonkey.o $(foreach obj,$(SM_OBJS),spidermonkey/$(obj))

all: all-recursive libecmascript.a
libecmascript.a: $(OBJS)

clean:
	rm -f *.o *.a

-include ../../Makefile.lib
