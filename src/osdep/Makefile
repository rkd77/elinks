path_to_top=../..
include $(path_to_top)/Makefile.config
INCLUDES += $(X_CFLAGS)

ifeq ($(CONFIG_BEOS),yes)
beosdir = beos
beosobj = $(beosdir)/lib.o
endif

ifeq ($(CONFIG_OS2),yes)
os2dir = os2
os2obj = $(os2dir)/lib.o
endif

ifeq ($(CONFIG_RISCOS),yes)
riscosdir = riscos
riscosobj = $(riscosdir)/lib.o
endif

ifeq ($(CONFIG_UNIX),yes)
unixdir = unix
unixobj = $(unixdir)/lib.o
endif

ifeq ($(CONFIG_WIN32),yes)
win32dir = win32
win32obj = $(win32dir)/lib.o
endif

SUBDIRS = $(beosdir) $(os2dir) $(riscosdir) $(unixdir) $(win32dir)

OBJS = \
 getifaddrs.o \
 newwin.o \
 osdep.o \
 signals.o \
 stub.o \
 sysname.o \
 $(beosobj) \
 $(os2obj) \
 $(riscosobj) \
 $(unixobj) \
 $(win32obj)

include $(path_to_top)/Makefile.lib
