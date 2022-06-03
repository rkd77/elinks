top_builddir=../..
include $(top_builddir)/Makefile.config

OBJS = cookies.o dialogs.o path.o parser.o
PROG = parsetst

TEST_PROGS = \
 cookies-t$(EXEEXT)

TESTDEPS = path.o

TESTDEPS-$(CONFIG_UTF8) += $(top_builddir)/src/intl/width.o


PARSETSTDEPS = \
 $(top_builddir)/src/util/error.o \
 $(top_builddir)/src/util/string.o

ifdef CONFIG_DEBUG
PARSETSTDEPS += $(top_builddir)/src/util/memdebug.o
endif

parsetst: $(PARSETSTDEPS) parser.o parsetst.o
	$(call cmd,link)

CLEAN += parsetst.o

include $(top_srcdir)/Makefile.lib
