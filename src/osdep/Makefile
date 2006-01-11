top_builddir=../..
include $(top_builddir)/Makefile.config
INCLUDES += $(X_CFLAGS)

SUBDIRS-$(CONFIG_BEOS)		+= beos
SUBDIRS-$(CONFIG_OS_OS2)		+= os2
SUBDIRS-$(CONFIG_RISCOS)	+= riscos
SUBDIRS-$(CONFIG_UNIX)		+= unix
SUBDIRS-$(CONFIG_WIN32)		+= win32

OBJS = \
 getifaddrs.o \
 newwin.o \
 osdep.o \
 signals.o \
 stub.o \
 sysname.o

include $(top_srcdir)/Makefile.lib
