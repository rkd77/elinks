top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(X_CFLAGS)

SUBDIRS-$(CONFIG_OS_BEOS)		+= beos
SUBDIRS-$(CONFIG_OS_OS2)		+= os2
SUBDIRS-$(CONFIG_OS_RISCOS)		+= riscos
SUBDIRS-$(CONFIG_OS_UNIX)		+= unix
SUBDIRS-$(CONFIG_OS_WIN32)		+= win32

OBJS = \
 getifaddrs.o \
 newwin.o \
 osdep.o \
 signals.o \
 stub.o \
 sysname.o

include $(top_srcdir)/Makefile.lib
