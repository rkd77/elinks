top_builddir=../../..
include $(top_builddir)/Makefile.config

# Disabled since it requires DOM_STACK_TRACE to be defined
#	dom-select
TEST_PROGS = \
 html-mangle \
 sgml-parser

TESTDEPS = \
 $(top_builddir)/src/dom/$(LIB_O_NAME)

include $(top_srcdir)/Makefile.lib
