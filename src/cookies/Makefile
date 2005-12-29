top_builddir=../..
include $(top_builddir)/Makefile.config

OBJS = cookies.o dialogs.o parser.o
PROG = parsetst

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
