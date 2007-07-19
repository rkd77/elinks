top_builddir=../../..
include $(top_builddir)/Makefile.config

TEST_PROGS = \
 test_uri

TESTDEPS = \
 $(top_builddir)/src/protocol/protocol.o \
 $(top_builddir)/src/protocol/uri.o \
 stub.o

CLEAN = stub.o

test_uri:: stub.o

include $(top_srcdir)/Makefile.lib
