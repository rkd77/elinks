top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS) $(MUJS_CFLAGS)

SUBDIRS-$(CONFIG_ECMASCRIPT_SMJS)	+= spidermonkey

SUBDIRS-$(CONFIG_LIBDOM)	+= libdom

SUBDIRS-$(CONFIG_MUJS)	+= mujs

SUBDIRS-$(CONFIG_QUICKJS)	+= quickjs

OBJS-$(CONFIG_ECMASCRIPT_SMJS)		+= css2xpath.obj ecmascript.obj localstorage-db.obj spidermonkey.obj

OBJS-$(CONFIG_MUJS)		+= css2xpath.obj ecmascript.obj localstorage-db.obj mujs.obj

OBJS-$(CONFIG_QUICKJS)		+= css2xpath.obj ecmascript.obj localstorage-db.obj quickjs.obj

ifeq ($(CONFIG_ECMASCRIPT_SMJS), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else ifeq ($(CONFIG_SCRIPTING_SPIDERMONKEY), yes)
CONFIG_ANY_SPIDERMONKEY = yes
else
CONFIG_ANY_SPIDERMONKEY = no
endif

OBJS-$(CONFIG_ANY_SPIDERMONKEY) += spidermonkey-shared.obj

include $(top_srcdir)/Makefile.lib
