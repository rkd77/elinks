-include Makefile.config
# This should be _really_ ., but only after we have src/Makefile.
# Otherwise, it wouldn't be set right in the submakefiles.
path_to_top = ..

# TODO: Automagically rerun autoconf.

SUBDIRS = doc po src


all: all-recursive

clean: clean-recursive
	rm -rf features.log

install: install-recursive

-include Makefile.lib
