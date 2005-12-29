top_builddir=../../..
include $(top_builddir)/Makefile.config

elinks.1: $(srcdir)elinks.1.in $(top_srcdir)/configure.in
	cd $(top_builddir) && \
	CONFIG_FILES="$(RELPATH)$@" CONFIG_HEADERS= $(SHELL) ./config.status

MAN1 = elinks.1

include $(top_srcdir)/Makefile.lib
