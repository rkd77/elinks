top_builddir=../../..
include $(top_builddir)/Makefile.config

PROG = sgml-parser

TESTDEPS = \
 $(top_builddir)/src/dom/lib.o \
 $(top_builddir)/src/util/error.o \
 $(top_builddir)/src/osdep/stub.o \
 $(top_builddir)/src/util/hash.o \
 $(top_builddir)/src/util/string.o \
 $(top_builddir)/src/util/memdebug.o \
 $(top_builddir)/src/util/memory.o

sgml-parser: $(TESTDEPS) sgml-parser.o
	$(call cmd,link)

test: sgml-parser
	./sgml-parser

CLEAN += sgml-parser.o

include $(top_srcdir)/Makefile.lib
