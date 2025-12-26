top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS) $(MUJS_CFLAGS)

SUBDIRS-$(CONFIG_LIBDOM)	+= libdom

SUBDIRS-$(CONFIG_MUJS)	+= mujs

SUBDIRS-$(CONFIG_QUICKJS)	+= quickjs

SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS)	+= spidermonkey

OBJS-$(CONFIG_ECMASCRIPT_SMJS)		+= ecmascript.obj ecmascript-c.obj localstorage-db.o spidermonkey.obj timer.obj

OBJS-$(CONFIG_MUJS)		+= ecmascript.o ecmascript-c.o localstorage-db.o mujs.o timer.obj

OBJS-$(CONFIG_QUICKJS)		+= ecmascript.o ecmascript-c.o localstorage-db.o quickjs.o timer.obj

ifeq ($(CONFIG_ECMASCRIPT_SMJS), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else ifeq ($(CONFIG_SCRIPTING_SPIDERMONKEY), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else
CONFIG_ANY_SPIDERMONKEY = no
endif

OBJS-$(CONFIG_ANY_SPIDERMONKEY) += spidermonkey-shared.obj

include $(top_srcdir)/Makefile.lib

$(shell xxd -i fetch.js fetch.h)
