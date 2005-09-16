path_to_top=../../..
include $(path_to_top)/Makefile.config

SUBDIRS = html

OBJS = sgml.o parser.o scanner.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
