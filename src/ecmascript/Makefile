top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS)	+= spidermonkey

SUBDIRS-$(CONFIG_QUICKJS)	+= quickjs

OBJS-$(CONFIG_ECMASCRIPT_SMJS)		+= css2xpath.o ecmascript.o localstorage-db.o spidermonkey.o

OBJS-$(CONFIG_QUICKJS)		+= css2xpath.o ecmascript.o localstorage-db.o quickjs.o

ifeq ($(CONFIG_ECMASCRIPT_SMJS), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else ifeq ($(CONFIG_SCRIPTING_SPIDERMONKEY), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else
CONFIG_ANY_SPIDERMONKEY = no
endif

OBJS-$(CONFIG_ANY_SPIDERMONKEY) += spidermonkey-shared.o

include $(top_srcdir)/Makefile.lib
