path_to_top = .
-include $(path_to_top)/Makefile.config

# TODO: Automagically rerun autoconf.

SUBDIRS = doc po src
CLEAN	= features.log

ifeq ($(wildcard Makefile.config),)
# Catch all
$(MAKECMDGOALS) default:
	@echo "You need to first run ./configure"
else
include $(path_to_top)/Makefile.lib
endif
