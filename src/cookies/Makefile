path_to_top=../..
include $(path_to_top)/Makefile.config

OBJS = cookies.o dialogs.o parser.o

all-l: libcookies.a
libcookies.a: $(OBJS)

parsetst: parser.o parsetst.o
	$(call cmd,link) -L../util/libutil.a

include $(path_to_top)/Makefile.lib
