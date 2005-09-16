path_to_top=../../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_MARKS),yes)
marksobj = marks.o
endif

OBJS = draw.o form.o link.o $(marksobj) search.o textarea.o view.o vs.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
