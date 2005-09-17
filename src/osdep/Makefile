path_to_top=../..
include $(path_to_top)/Makefile.config
INCLUDES += $(X_CFLAGS)

ifeq ($(CONFIG_BEOS),yes)
beosdir = beos
beosobj = $(beosdir)/beos.o $(beosdir)/overrides.o
endif

ifeq ($(CONFIG_OS2),yes)
os2dir = os2
os2obj = $(os2dir)/os2.o
endif

ifeq ($(CONFIG_RISCOS),yes)
riscosdir = riscos
riscosobj = $(riscosdir)/riscos.o
endif

ifeq ($(CONFIG_UNIX),yes)
unixdir = unix
unixobj = $(unixdir)/bsd.o $(unixdir)/unix.o
endif

ifeq ($(CONFIG_WIN32),yes)
win32dir = win32
win32obj = $(win32dir)/win32.o $(win32dir)/overrides.o
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

all-l: libosdep.a
libosdep.a: $(OBJS)

include $(path_to_top)/Makefile.lib
