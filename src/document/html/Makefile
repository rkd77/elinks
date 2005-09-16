path_to_top=../../..
include $(path_to_top)/Makefile.config

SUBDIRS = parser

OBJS = frames.o parser.o renderer.o tables.o

# Do not forget to also add the .o to ../Makefile. Yes, it sucks.
all-l: $(OBJS)

include $(path_to_top)/Makefile.lib
