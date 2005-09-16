path_to_top=../../..
include $(path_to_top)/Makefile.config
INCLUDES += $(SPIDERMONKEY_CFLAGS)

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
OBJS = document.o form.o location.o navigator.o unibar.o window.o

all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
