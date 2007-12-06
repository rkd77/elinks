top_builddir=.
-include $(top_builddir)/Makefile.config

SUBDIRS = doc src
SUBDIRS-$(CONFIG_NLS) += po
CLEAN	= features.log
calltest = $1-$1

all-recursive: config.h

# Automagically rerun autotools
$(top_builddir)/config.status: $(top_srcdir)/configure
	cd $(top_builddir) && $(SHELL) ./config.status --recheck

ACLOCAL_M4 = $(top_srcdir)/aclocal.m4
$(ACLOCAL_M4): $(top_srcdir)/configure.in $(top_srcdir)/acinclude.m4
	cd $(top_srcdir) && $(ACLOCAL)

$(top_srcdir)/configure: $(top_srcdir)/configure.in $(ACLOCAL_M4)
	cd $(top_srcdir) && $(AUTOCONF)

# Makefile.config doesn't need a separate timestamp file because
# touching it doesn't directly cause other files to be rebuilt.
$(top_builddir)/Makefile.config: $(top_srcdir)/Makefile.config.in $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=Makefile.config CONFIG_HEADERS= \
	     $(SHELL) ./config.status

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


# Makefile.lib heavily uses $(call ...), which was added in GNU Make 3.78.
# An elinks-users post on 2007-12-04 reported trouble with GNU Make 3.68.
# Detect this situation and give an error message.  $(MAKECMDGOALS) is
# only defined by 3.76 and later, so specify the "all" target as well.
# This check has been tested with GNU Make 3.68, 3.77, 3.78.1, and 3.81.
ifneq ($(call calltest,ok),ok-ok)
$(MAKECMDGOALS) default all:
	@echo >&2 "You need GNU Make 3.78 or later"
	@false
else
ifeq ($(wildcard Makefile.config),)
# Catch all
$(MAKECMDGOALS) default:
	@echo "You need to first run ./configure"
else
include $(top_srcdir)/Makefile.lib
endif
endif
