path_to_top=../../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_CGI),yes)
cgiobj = cgi.o
endif

OBJS = file.o $(cgiobj)

include $(path_to_top)/Makefile.lib
