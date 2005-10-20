path_to_top=../../..
include $(path_to_top)/Makefile.config

elinks.1: $(srcdir)/elinks.1.in $(top_srcdir)/configure.in
	cd $(top_srcdir) && \
	CONFIG_FILES="$(RELPATH)$@" CONFIG_HEADERS= $(SHELL) ./config.status

MAN1 = elinks.1

include $(top_srcdir)/Makefile.lib
