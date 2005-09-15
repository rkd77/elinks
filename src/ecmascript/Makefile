-include ../../Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

SUBDIRS = spidermonkey

SM_OBJS = document.o form.o location.o navigator.o unibar.o window.o
OBJS = ecmascript.o spidermonkey.o $(foreach obj,$(SM_OBJS),spidermonkey/$(obj))

all-l: libecmascript.a
libecmascript.a: $(OBJS)

-include ../../Makefile.lib
