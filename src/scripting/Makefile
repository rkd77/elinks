path_to_top=../..
include $(path_to_top)/Makefile.config

SUBDIRS-$(CONFIG_GUILE)		+= guile
SUBDIRS-$(CONFIG_LUA)		+= lua
SUBDIRS-$(CONFIG_PERL)		+= perl
SUBDIRS-$(CONFIG_PYTHON)	+= python
SUBDIRS-$(CONFIG_RUBY)		+= ruby

OBJS = scripting.o

include $(top_srcdir)/Makefile.lib
