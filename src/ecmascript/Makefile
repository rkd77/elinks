top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS) $(MUJS_CFLAGS)

SUBDIRS-$(CONFIG_LIBDOM)	+= libdom

SUBDIRS-$(CONFIG_MUJS)	+= mujs

OBJS-$(CONFIG_ECMASCRIPT_SMJS)		+= ecmascript.obj localstorage-db.obj spidermonkey.obj timer.obj

OBJS-$(CONFIG_MUJS)		+= ecmascript.obj localstorage-db.obj mujs.obj timer.obj

OBJS-$(CONFIG_QUICKJS)		+= ecmascript.obj localstorage-db.obj quickjs.obj timer.obj

ifeq ($(CONFIG_ECMASCRIPT_SMJS), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else ifeq ($(CONFIG_SCRIPTING_SPIDERMONKEY), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else
CONFIG_ANY_SPIDERMONKEY = no
endif

OBJS-$(CONFIG_ANY_SPIDERMONKEY) += spidermonkey-shared.obj

include $(top_srcdir)/Makefile.lib
