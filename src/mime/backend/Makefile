top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS-$(CONFIG_MAILCAP)		+= mailcap.o
OBJS-$(CONFIG_MIMETYPES)	+= mimetypes.o

OBJS = common.o default.o

TEST_PROGS = \
 mailcap-cache

# The dependencies are a bit funny here! I don't know why. Just remember to
# make clean before making the test. --jonas
mailcap-cache.o: mailcap.c
	$(call cmd,compile,-DTEST_MAILCAP)

TESTDEPS = \
 common.o \
 $(top_builddir)/src/osdep/osdep.o \
 $(top_builddir)/src/osdep/stub.o \
 $(top_builddir)/src/util/conv.o \
 $(top_builddir)/src/util/error.o \
 $(top_builddir)/src/util/file.o \
 $(top_builddir)/src/util/hash.o \
 $(top_builddir)/src/util/memory.o \
 $(top_builddir)/src/util/string.o \
 $(top_builddir)/src/util/time.o

TESTDEPS-$(CONFIG_NLS)	 += $(top_builddir)/src/intl/gettext/lib.o
TESTDEPS-$(CONFIG_DEBUG) += $(top_builddir)/src/util/memdebug.o

include $(top_srcdir)/Makefile.lib
