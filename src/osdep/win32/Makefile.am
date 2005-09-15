## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.4.6.1 2005/02/28 01:20:29 jonas Exp $

include $(top_srcdir)/Makefile.base

# We don't want to expose this outside of /src/osdep/ at all, and we can't
# merge the whole library of these with libosdep.a. So we only compile
# objects here and link them directly to libosdep.a in ../Makefile.am.

# However, we apparently need to create the library in order to convince
# automake to compile the objects :/. We DON'T use libsystem.a ANYWHERE.

noinst_LIBRARIES = libsystem.a
libsystem_a_SOURCES = \
 overrides.c \
 overrides.h \
 sysinfo.h \
 win32.c \
 win32.h
