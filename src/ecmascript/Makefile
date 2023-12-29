top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS) $(MUJS_CFLAGS)

SUBDIRS-$(CONFIG_LIBDOM)	+= libdom

SUBDIRS-$(CONFIG_MUJS)	+= mujs

SUBDIRS-$(CONFIG_QUICKJS)	+= quickjs

SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS)	+= spidermonkey

OBJS-$(CONFIG_ECMASCRIPT_SMJS)		+= ecmascript.obj ecmascript-c.obj localstorage-db.obj spidermonkey.obj timer.o

OBJS-$(CONFIG_MUJS)		+= ecmascript.obj ecmascript-c.obj localstorage-db.obj mujs.obj timer.o

OBJS-$(CONFIG_QUICKJS)		+= ecmascript.obj ecmascript-c.obj localstorage-db.obj quickjs.obj timer.o

ifeq ($(CONFIG_ECMASCRIPT_SMJS), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else ifeq ($(CONFIG_SCRIPTING_SPIDERMONKEY), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else
CONFIG_ANY_SPIDERMONKEY = no
endif

OBJS-$(CONFIG_ANY_SPIDERMONKEY) += spidermonkey-shared.obj

include $(top_srcdir)/Makefile.lib
