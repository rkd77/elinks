## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.40.6.3 2005/04/13 23:30:14 jonas Exp $

SUBDIRS = Unicode contrib debian doc intl po src test

EXTRA_DIST = ABOUT-NLS BUGS TODO SITES Makefile.base autogen.sh features.conf m4/*.m4

install-exec-hook:
	@echo "**************************************************************"
	@echo "If you are upgrading from Links or ELinks <= 0.4pre7, you may"
	@echo "want to check out INSTALL for some hints; ie. you may want to"
	@echo "make symlink 'links' pointing to 'elinks'."
	@echo "**************************************************************"


CLEANFILES = $(srcdir)/features.log
MAINTAINERCLEANFILES = $(srcdir)/acinclude.m4
