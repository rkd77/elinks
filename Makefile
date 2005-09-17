path_to_top = .
include $(path_to_top)/Makefile.config

# TODO: Automagically rerun autoconf.

SUBDIRS = doc po src


clean-l:
	rm -rf features.log

include $(path_to_top)/Makefile.lib
