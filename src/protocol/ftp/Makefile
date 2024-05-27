top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS = ftp.o

TEST_PROGS = \
 ftp-parser$(EXEEXT)

TESTDEPS = \
 $(top_builddir)/src/protocol/date.o \
 $(top_builddir)/src/protocol/ftpparse.o

include $(top_srcdir)/Makefile.lib
