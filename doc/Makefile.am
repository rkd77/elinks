## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.22.2.2 2005/05/03 17:31:54 jonas Exp $

SUBDIRS = man

EXTRA_DIST = \
 README \
 color-model.txt \
 ecmascript.txt \
 elinks-lua.texi \
 events.txt \
 faq.html \
 feedback.txt \
 hacking.txt \
 index.txt \
 manual.txt \
 manual.html \
 perl.pod \
 small.txt

ELINKS=$(top_builddir)/src/elinks

update-manpages:
	@if test ! -x "$(ELINKS)";                                            \
	then                                                                  \
		echo "There is no '$(ELINKS)' to update from.";               \
	else                                                                  \
		sh $(top_srcdir)/doc/tools/help2doc --elinks=$(ELINKS)        \
				  --elinksconf                                \
				  > $(top_srcdir)/doc/man/man5/elinks.conf.5; \
	fi
