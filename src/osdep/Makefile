path_to_top=../..
include $(path_to_top)/Makefile.config
INCLUDES += $(X_CFLAGS)

ifeq ($(CONFIG_BEOS),yes)
beosdir = beos
endif

ifeq ($(CONFIG_OS2),yes)
os2dir = os2
endif

ifeq ($(CONFIG_RISCOS),yes)
riscosdir = riscos
endif

ifeq ($(CONFIG_UNIX),yes)
unixdir = unix
endif

ifeq ($(CONFIG_WIN32),yes)
win32dir = win32
endif

SUBDIRS = $(beosdir) $(os2dir) $(riscosdir) $(unixdir) $(win32dir)

OBJS = \
 getifaddrs.o \
 newwin.o \
 osdep.o \
 signals.o \
 stub.o \
 sysname.o

include $(path_to_top)/Makefile.lib
