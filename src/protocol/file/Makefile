path_to_top=../../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_CGI),yes)
cgiobj = cgi.o
endif

OBJS = file.o $(cgiobj)

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
