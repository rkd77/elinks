top_builddir=..
include $(top_builddir)/Makefile.config

SUBDIRS = man

# A little trick to simplify some of the rules.
VPATH = $(builddir):$(srcdir):$(top_srcdir)/contrib/perl

docdir = $(datadir)/doc

# Used by install-doc
HTML_DIR = $(DESTDIR)$(docdir)/$(PACKAGE)/html
PDF_DIR  = $(DESTDIR)$(docdir)/$(PACKAGE)/pdf
TXT_DIR  = $(DESTDIR)$(docdir)/$(PACKAGE)/txt

ASCIIDOC_CONF  = $(srcdir)asciidoc.conf
ASCIIDOC_FLAGS += --no-conf -f $(srcdir)tools/asciidoc/asciidoc.conf \
		  -f $(srcdir)tools/asciidoc/$(call backend).conf \
		  -f $(ASCIIDOC_CONF) \
		  -a "builddir=$(CURDIR)/" \
		  -a elinks_version=$(VERSION)

#############################################################################
# Build files

TXT_DOCS_NOINSTALL = \
 features.txt \
 keymap-actions.txt \
 keymap-defaults.txt \

TXT_DOCS_ASIS = \
 python.txt

HTML_DOCS-$(CONFIG_ASCIIDOC) += \
 elinks.1.html \
 elinks.conf.5.html \
 elinkskeys.5.html \
 hacking.html \
 manual.html

HTML_DOCS-$(CONFIG_XMLTO) += \
 manual.html-chunked

HTML_DOCS-$(CONFIG_POD2HTML) += \
 perl.html \
 perl-hooks.html

# We don't nowadays run pod2html on the po/perl/ scripts, because
# "make install" does not install them and they do not have the .pl
# suffix expected by the pod2html rule below.

MAN_DOCS-$(CONFIG_XMLTO) += \
 elinks.1 \
 elinks.conf.5 \
 elinkskeys.5

# Use jw for generating PDF, since xmlto seems to freak out.
PDF_DOCS-$(CONFIG_JW) += \
 manual.pdf

API_DOCS-$(CONFIG_DOXYGEN) += \
 api/doxygen

#############################################################################
## Set the default doc rules

MAN_DOCS  = $(MAN_DOCS-yes)
HTML_DOCS = $(HTML_DOCS-yes)
PDF_DOCS  = $(PDF_DOCS-yes)
API_DOCS  = $(API_DOCS-yes)

txt:  $(TXT_DOCS_NOINSTALL)
html: txt $(HTML_DOCS)
pdf:  txt $(PDF_DOCS)
man:  txt $(MAN_DOCS)
api:  $(API_DOCS)

all-docs: man html pdf

install-doc: all-docs update-man install
	@$(foreach doc,$(HTML_DOCS), \
		if test -d $(doc); then \
			$(MKINSTALLDIRS) $(DESTDIR)$(docdir)/$(PACKAGE)/html/$(doc); \
			$(call ncmd,installdata,$(doc)/*,$(HTML_DIR)/$(doc)); \
		else \
			$(MKINSTALLDIRS) $(DESTDIR)$(docdir)/$(PACKAGE)/html; \
			$(call ncmd,installdata,$(doc),$(HTML_DIR)); \
		fi;)
	@$(foreach doc,$(PDF_DOCS), \
		$(MKINSTALLDIRS) $(DESTDIR)$(docdir)/$(PACKAGE)/pdf; \
		$(call ncmd,installdata,$(doc),$(PDF_DIR));)
	@$(foreach doc,$(TXT_DOCS_ASIS), \
		$(MKINSTALLDIRS) $(DESTDIR)$(docdir)/$(PACKAGE)/txt; \
		$(call ncmd,installdata,$(srcdir)$(doc),$(TXT_DIR));)

update-man: man
	@$(if $(MAN_DOCS), \
		$(call ncmd,installdata,elinks.1,$(srcdir)man/man1/elinks.1.in); \
		$(call ncmd,installdata,elinkskeys.5,$(srcdir)man/man5/); \
		$(call ncmd,installdata,elinks.conf.5,$(srcdir)man/man5/))

clean-local:
	@$(RM) -r api $(TXT_DOCS_NOINSTALL) $(MAN_DOCS) $(HTML_DOCS) $(PDF_DOCS) *.tmp *.xml

# TODO: perl.pod should be pod2ized during make install. --pasky
install-local:

#############################################################################
# Generated asciidoc files

# Scripts and Dependencies
HELP2XML = $(srcdir)tools/help2xml
CONF2DOC = $(srcdir)tools/conf2doc
KEYS2DOC = $(srcdir)tools/keys2doc
ELINKS	 = $(top_builddir)/src/elinks
FEATURES = $(top_srcdir)/features.conf
KBDBIND	 = $(top_srcdir)/src/config/kbdbind.c

# Locale env vars to override system one to ensure commands
# using elinks binary will generate texts in english
LOCALES = LC_ALL=C LANGUAGE=en

# FIXME: Keep generated .txt files relative to the source directory and files
# they are included in.
quiet_cmd_help2xml = ' [$(LINK_COLOR)HELP2XML$(END_COLOR)]   $(RELPATH)$@'
      cmd_help2xml = $(LOCALES) $(HELP2XML) $(ELINKS) $@

quiet_cmd_conf2doc = ' [$(LINK_COLOR)CONF2DOC$(END_COLOR)]   $(RELPATH)$@'
      cmd_conf2doc = $(LOCALES) $(CONF2DOC) $(FEATURES) > $@

quiet_cmd_keys2doc = ' [$(LINK_COLOR)KEYS2DOC$(END_COLOR)]   $(RELPATH)$@'
      cmd_keys2doc = $(LOCALES) $(KEYS2DOC) $(KBDBIND) $@ > $@

features.txt: $(FEATURES) $(CONF2DOC)
	$(call cmd,conf2doc)

keymap-%.txt: $(KBDBIND) $(KEYS2DOC)
	$(call cmd,keys2doc)

option-%.frag.xml: $(ELINKS) $(HELP2XML)
	$(call cmd,help2xml)

option-%.frag.xhtml: $(ELINKS) $(HELP2XML)
	$(call cmd,help2xml)

#############################################################################
# Build commands and macros

      quiet_cmd_jw = '       [$(LINK_COLOR)JW$(END_COLOR)]   $(RELPATH)$@'
            cmd_jw = $(JW) -b $(2) $<

   quiet_cmd_xmlto = '    [$(LINK_COLOR)XMLTO$(END_COLOR)]   $(RELPATH)$@'
         cmd_xmlto = $(XMLTO) -o $(call outdir) $(3) $(2) $<

quiet_cmd_pod2html = ' [$(LINK_COLOR)POD2HTML$(END_COLOR)]   $(RELPATH)$@'
      cmd_pod2html = $(POD2HTML) --outfile=$@ < $<

quiet_cmd_asciidoc = ' [$(LINK_COLOR)ASCIIDOC$(END_COLOR)]   $(RELPATH)$@'
      cmd_asciidoc = $(ASCIIDOC) $(ASCIIDOC_FLAGS) -b $(call backend) -d $(call doctype) -o $@ $<

 quiet_cmd_doxygen = ' [$(LINK_COLOR)DOXYGEN$(END_COLOR)]   $(RELPATH)$@'
       cmd_doxygen = $(DOXYGEN) $(2)

# Based on $@ find out asciidoc doctype or backend + xmlto output dir.
doctype = $(if $(findstring .1.,$@)$(findstring .5.,$@),manpage,book)
backend = $(if $(findstring .xml,$@),docbook,xhtml11)
outdir  = $(if $(findstring -chunked,$@),$@,.)

# Loosely track dependencies via asciidoc includes.
asciidoc_dep = sed -n 's/[{]builddir}//g;s@include1\{0,1\}::\(.*\)\[.*@$@: $< \1@p' < $< > .deps/$(@F).asciidoc

-include .deps/*.asciidoc


#############################################################################
# Build recipies

%.html: %.txt $(ASCIIDOC_CONF)
	$(call cmd,asciidoc,xhtml11)
	@-$(call asciidoc_dep)

%.xml: %.txt $(ASCIIDOC_CONF)
	$(call cmd,asciidoc,docbook)
	@-$(call asciidoc_dep)

# asciidoc_dep above also generates these dependencies, but it runs
# only after asciidoc has succeeded, which won't happen if the files
# are missing.
elinks.1.xml: option-command.frag.xml
elinks.conf.5.xml: option-config.frag.xml
elinks.1.html: option-command.frag.xhtml
elinks.conf.5.html: option-config.frag.xhtml

%.1: %.1.xml
	$(call cmd,xmlto,man)

%.5: %.5.xml
	$(call cmd,xmlto,man)

%.html-chunked: %.xml
	$(call cmd,xmlto,html)

%.pdf: %.xml
	$(call cmd,jw,pdf)

%.html: %.pod
	$(call cmd,pod2html)

perl-%.html: %.pl
	$(call cmd,pod2html)

Doxyfile: $(srcdir)Doxyfile.in $(top_srcdir)/configure.in
	cd $(top_builddir) && \
	CONFIG_FILES="$(RELPATH)$@" CONFIG_HEADERS= $(SHELL) ./config.status

api/doxygen: Doxyfile $(shell find $(top_srcdir)/src/ -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort)
	$(call cmd,doxygen,$<)

include $(top_srcdir)/Makefile.lib
