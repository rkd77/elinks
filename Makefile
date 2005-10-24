top_builddir=.
-include $(top_builddir)/Makefile.config

SUBDIRS = doc po src
CLEAN	= features.log

all-recursive: config.h

# Automagically rerun autotools
$(top_builddir)/config.status: $(top_srcdir)/configure
	cd $(top_builddir) && $(SHELL) ./config.status --recheck

ACLOCAL_M4 = $(top_srcdir)/aclocal.m4
$(ACLOCAL_M4): $(top_srcdir)/configure.in $(top_srcdir)/acinclude.m4
	cd $(top_srcdir) && $(ACLOCAL)

$(top_srcdir)/configure: $(top_srcdir)/configure.in $(ACLOCAL_M4)
	cd $(top_srcdir) && $(AUTOCONF)

$(top_builddir)/config.h: $(top_builddir)/stamp-h
	@cd $(top_builddir) && \
	if test ! -f $@; then \
		rm -f stamp-h; \
		$(MAKE) stamp-h; \
	else :; fi

$(top_builddir)/stamp-h: $(top_srcdir)/config.h.in $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES= CONFIG_HEADERS=config.h \
	     $(SHELL) ./config.status
	@echo timestamp > stamp-h 2> /dev/null

$(top_srcdir)/config.h.in: $(top_srcdir)/stamp-h.in
	@if test ! -f $@; then \
		rm -f $(top_srcdir)/stamp-h.in; \
		$(MAKE) $(top_srcdir)/stamp-h.in; \
	else :; fi

$(top_srcdir)/stamp-h.in: $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOHEADER)
	@echo timestamp > $(top_srcdir)/stamp-h.in 2> /dev/null


ifeq ($(wildcard Makefile.config),)
# Catch all
$(MAKECMDGOALS) default:
	@echo "You need to first run ./configure"
else
include $(top_srcdir)/Makefile.lib
endif
