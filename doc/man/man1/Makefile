path_to_top=../../..
include $(path_to_top)/Makefile.config

elinks.1: elinks.1.in $(path_to_top)/configure.in
	cd $(path_to_top) && \
	CONFIG_FILES="$(RELPATH)$@" CONFIG_HEADERS= $(SHELL) ./config.status

MAN1 = elinks.1

include $(top_srcdir)/Makefile.lib
