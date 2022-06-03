top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS = ftp.o parse.o

TEST_PROGS = \
 ftp-parser$(EXEEXT)

TESTDEPS = \
 $(top_builddir)/src/protocol/date.o \
 $(top_builddir)/src/protocol/ftp/parse.o

TESTDEPS-$(CONFIG_UTF8) += $(top_builddir)/src/intl/width.o

include $(top_srcdir)/Makefile.lib
