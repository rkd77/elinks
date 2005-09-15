## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.28.6.2 2005/09/14 11:36:12 jonas Exp $

include $(top_srcdir)/Makefile.base

if CONFIG_BEOS
beosdir = beos
beosobj = $(beosdir)/beos.o $(beosdir)/overrides.o
endif

if CONFIG_OS2
os2dir = os2
os2obj = $(os2dir)/os2.o
endif

if CONFIG_RISCOS
riscosdir = riscos
riscosobj = $(riscosdir)/riscos.o
endif

if CONFIG_UNIX
unixdir = unix
unixobj = $(unixdir)/unix.o
endif

if CONFIG_WIN32
win32dir = win32
win32obj = $(win32dir)/win32.o $(win32dir)/overrides.o
endif

SUBDIRS = $(beosdir) $(os2dir) $(riscosdir) $(unixdir) $(win32dir)

noinst_LIBRARIES = libosdep.a

libosdep_a_LIBADD = \
 $(beosobj) \
 $(os2obj) \
 $(riscosobj) \
 $(unixobj) \
 $(win32obj)

libosdep_a_SOURCES = \
 ascii.h \
 generic.h \
 getifaddrs.c \
 getifaddrs.h \
 newwin.c \
 newwin.h \
 osdep.c \
 osdep.h \
 stat.h \
 stub.c \
 stub.h \
 system.h

