top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS-$(CONFIG_MAILCAP)		+= mailcap.o
OBJS-$(CONFIG_MIMETYPES)	+= mimetypes.o

OBJS = common.o default.o

TEST_PROGS = \
 mailcap-cache$(EXEEXT)

# The dependencies are a bit funny here! I don't know why. Just remember to
# make clean before making the test. --jonas
mailcap-cache.o: $(top_srcdir)/src/mime/backend/mailcap.c
	$(call cmd,compile,-DTEST_MAILCAP)

TESTDEPS = \
 common.o \
 $(top_builddir)/src/osdep/osdep.o

TESTDEPS-$(CONFIG_NLS)	 += $(top_builddir)/src/intl/gettext/lib.o

include $(top_srcdir)/Makefile.lib
