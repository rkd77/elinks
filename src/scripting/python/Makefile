path_to_top=../../..
include $(path_to_top)/Makefile.config

INCLUDES += $(PYTHON_CFLAGS)

OBJS = python.o hooks.o core.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
