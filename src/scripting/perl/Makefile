path_to_top=../../..
include $(path_to_top)/Makefile.config

INCLUDES += $(PERL_CFLAGS)

OBJS = perl.o hooks.o core.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-local: $(OBJS)

include $(path_to_top)/Makefile.lib
