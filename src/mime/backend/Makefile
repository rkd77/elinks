top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS-$(CONFIG_DGI)	+= dgi.o
OBJS-$(CONFIG_MAILCAP)		+= mailcap.o
OBJS-$(CONFIG_MIMETYPES)	+= mimetypes.o

OBJS = common.o default.o

TEST_PROGS = \
 mailcap-cache$(EXEEXT)

mailcap-cache.o: $(top_srcdir)/src/mime/backend/mailcap.c
	$(call cmd,compile,-DTEST_MAILCAP)

TESTDEPS = \
 common.o \
 $(top_builddir)/src/osdep/osdep.o

TESTDEPS-$(CONFIG_NLS)	 += $(top_builddir)/src/intl/gettext/lib.o
TESTDEPS-$(CONFIG_GETTEXT) += $(top_builddir)/src/intl/libintl.o $(top_builddir)/src/util/env.o

include $(top_srcdir)/Makefile.lib
