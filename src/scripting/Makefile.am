## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.12 2005/06/02 18:01:34 witekfl Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_GUILE
guiledir = guile
guileobj = $(guiledir)/core.o $(guiledir)/guile.o $(guiledir)/hooks.o
endif

if CONFIG_LUA
luadir = lua
luaobj = $(luadir)/core.o $(luadir)/hooks.o $(luadir)/lua.o
endif

if CONFIG_PERL
perldir = perl
perlobj = $(perldir)/core.o $(perldir)/hooks.o $(perldir)/perl.o
endif

if CONFIG_PYTHON
pythondir = python
pythonobj = $(pythondir)/core.o $(pythondir)/hooks.o $(pythondir)/python.o
endif

if CONFIG_RUBY
rubydir = ruby
rubyobj = $(rubydir)/core.o $(rubydir)/hooks.o $(rubydir)/ruby.o
endif

SUBDIRS = $(guiledir) $(luadir) $(perldir) $(pythondir) $(rubydir)

EXTRA_DIST = TODO

noinst_LIBRARIES = libscripting.a

libscripting_a_LIBADD = \
 $(guileobj) \
 $(luaobj) \
 $(perlobj) \
 $(pythonobj) \
 $(rubyobj)

libscripting_a_SOURCES = \
 scripting.c \
 scripting.h

