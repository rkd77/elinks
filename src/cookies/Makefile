top_builddir=../..
include $(top_builddir)/Makefile.config

OBJS = cookies.o dialogs.o parser.o

parsetst: parser.o parsetst.o
	$(call cmd,link) -L../util/libutil.a

include $(top_srcdir)/Makefile.lib
