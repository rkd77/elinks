top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS = ftp.o parse.o

TEST_PROGS = \
 ftp-parser

TESTDEPS = \
 $(top_builddir)/src/protocol/date.o \
 $(top_builddir)/src/protocol/ftp/parse.o

include $(top_srcdir)/Makefile.lib
