top_builddir=../../../..
include $(top_builddir)/Makefile.config

SUBDIRS = 
TEST_PROGS = parse-meta-refresh-test
TESTDEPS += \
 $(top_builddir)/src/document/html/parse-meta-refresh.o

TESTDEPS-$(CONFIG_UTF8) += $(top_builddir)/src/intl/width.o


include $(top_srcdir)/Makefile.lib
