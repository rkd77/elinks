path_to_top=../../..
include $(path_to_top)/Makefile.config

INCLUDES += $(RUBY_CFLAGS)

OBJS = ruby.o hooks.o core.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-local: $(OBJS)

include $(path_to_top)/Makefile.lib
