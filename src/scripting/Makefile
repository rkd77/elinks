path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_GUILE),yes)
guiledir = guile
guileobj = $(guiledir)/core.o $(guiledir)/guile.o $(guiledir)/hooks.o
endif

ifeq ($(CONFIG_LUA),yes)
luadir = lua
luaobj = $(luadir)/core.o $(luadir)/hooks.o $(luadir)/lua.o
endif

ifeq ($(CONFIG_PERL),yes)
perldir = perl
perlobj = $(perldir)/core.o $(perldir)/hooks.o $(perldir)/perl.o
endif

ifeq ($(CONFIG_PYTHON),yes)
pythondir = python
pythonobj = $(pythondir)/core.o $(pythondir)/hooks.o $(pythondir)/python.o
endif

ifeq ($(CONFIG_RUBY),yes)
rubydir = ruby
rubyobj = $(rubydir)/core.o $(rubydir)/hooks.o $(rubydir)/ruby.o
endif

SUBDIRS = $(guiledir) $(luadir) $(perldir) $(pythondir) $(rubydir)

OBJS = scripting.o $(guileobj) $(luaobj) $(perlobj) $(pythonobj) $(rubyobj)

all-local: libscripting.a
libscripting.a: $(OBJS)

include $(path_to_top)/Makefile.lib
