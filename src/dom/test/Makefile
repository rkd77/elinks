top_builddir=../../..
include $(top_builddir)/Makefile.config

TEST_PROGS = \
 sgml-parser

TESTDEPS = \
 $(top_builddir)/src/dom/lib.o \
 $(top_builddir)/src/util/error.o \
 $(top_builddir)/src/osdep/stub.o \
 $(top_builddir)/src/util/hash.o \
 $(top_builddir)/src/util/memdebug.o \
 $(top_builddir)/src/util/memory.o

$(TEST_PROGS): $(TESTDEPS) $$@.o
	$(call cmd,link)

TESTS = $(wildcard test-*)

$(TESTS): $(TEST_PROGS)
	@echo "*** $@ ***"; $(call shellquote,$(SHELL)) $@ $(TEST_OPTS)

test: sgml-parser $(TESTS)

clean-local:
	@rm -fr trash

CLEAN += $(TEST_PROGS) $(patsubst %,%.o,$(TEST_PROGS))

.PHONY: $(TESTS)
.NOPARALLEL:

include $(top_srcdir)/Makefile.lib

# Shell quote;
# Result of this needs to be placed inside ''
# XXX: Placed here because Vim cannot highlight things right afterwards
shq = $(subst ','\'',$(1))
# This has surrounding ''
shellquote = '$(call shq,$(1))'
