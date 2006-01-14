top_builddir=../..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_SCRIPTING_GUILE)		+= guile
SUBDIRS-$(CONFIG_SCRIPTING_LUA)			+= lua
SUBDIRS-$(CONFIG_SCRIPTING_PERL)		+= perl
SUBDIRS-$(CONFIG_SCRIPTING_PYTHON)		+= python
SUBDIRS-$(CONFIG_SCRIPTING_RUBY)		+= ruby
SUBDIRS-$(CONFIG_SCRIPTING_SPIDERMONKEY)	+= smjs

OBJS = scripting.o

include $(top_srcdir)/Makefile.lib
