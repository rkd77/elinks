top_builddir=../../..
include $(top_builddir)/Makefile.config

OBJS = ftp.o parse.o

TEST_PROGS = \
 ftp-parser

TESTDEPS = \
 $(top_builddir)/src/intl/charsets.o \
 $(top_builddir)/src/osdep/stub.o \
 $(top_builddir)/src/protocol/date.o \
 $(top_builddir)/src/protocol/ftp/parse.o \
 $(top_builddir)/src/util/conv.o \
 $(top_builddir)/src/util/error.o \
 $(top_builddir)/src/util/hash.o \
 $(top_builddir)/src/util/memory.o \
 $(top_builddir)/src/util/string.o \
 $(top_builddir)/src/util/time.o

TESTDEPS-$(call not,$(CONFIG_SMALL)) += \
 $(top_builddir)/src/util/fastfind.o \

include $(top_srcdir)/Makefile.lib
