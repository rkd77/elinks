path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_GUILE),yes)
guiledir = guile
endif

ifeq ($(CONFIG_LUA),yes)
luadir = lua
endif

ifeq ($(CONFIG_PERL),yes)
perldir = perl
endif

ifeq ($(CONFIG_PYTHON),yes)
pythondir = python
endif

ifeq ($(CONFIG_RUBY),yes)
rubydir = ruby
endif

SUBDIRS = $(guiledir) $(luadir) $(perldir) $(pythondir) $(rubydir)

OBJS = scripting.o

include $(path_to_top)/Makefile.lib
