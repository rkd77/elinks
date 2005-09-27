path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_GUILE),yes)
guiledir = guile
guileobj = $(guiledir)/lib.o
endif

ifeq ($(CONFIG_LUA),yes)
luadir = lua
luaobj = $(luadir)/lib.o
endif

ifeq ($(CONFIG_PERL),yes)
perldir = perl
perlobj = $(perldir)/lib.o
endif

ifeq ($(CONFIG_PYTHON),yes)
pythondir = python
pythonobj = $(pythondir)/lib.o
endif

ifeq ($(CONFIG_RUBY),yes)
rubydir = ruby
rubyobj = $(rubydir)/lib.o
endif

SUBDIRS = $(guiledir) $(luadir) $(perldir) $(pythondir) $(rubydir)

OBJS = scripting.o $(guileobj) $(luaobj) $(perlobj) $(pythonobj) $(rubyobj)

include $(path_to_top)/Makefile.lib
