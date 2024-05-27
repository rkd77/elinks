top_builddir=../../..
include $(top_builddir)/Makefile.config

TEST_PROGS = \
 uri-test$(EXEEXT)

TESTDEPS = \
 $(top_builddir)/src/protocol/protocol.o \
 $(top_builddir)/src/protocol/uri.o \
 stub.o

CLEAN = stub.o

uri-test:: stub.o

include $(top_srcdir)/Makefile.lib
