-include ../Makefile.config

# Where to install the catalog files.
localedir = $(datadir)/locale

# These are for some reason not added magically to the Makefile.
MSGMERGE = msgmerge

POTFILES_REL = $(shell find $(top_srcdir)/src/ -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort)
POTFILES_ABS = $(shell cd $(top_srcdir); find src/ -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort)


SUFFIXES = .po .gmo .mo

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	@file=`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && echo -n $*": " \
	  && $(GMSGFMT) --statistics -o $$file $<


### The default rule
#
# Note: Target `all-local' must not depend on target '$(srcdir)/$(PACKAGE).pot',
# otherwise packages like GCC can not be built if only parts of the source have
# been downloaded.
#
# Note 2: We aren't gcc, we are ELinks. And I want that dependency ;-). --pasky
#
# Note 3: I changed my mind, it is really annoying if you just insert one line
# and suddenly you have tons of changes in all the .po files updating the line
# informations. --pasky
#
# $(CATALOGS) is empty if --disable-nls. Beats using *-(yes|no) targets.

all-l: $(CATALOGS)


### Creating the template po template file
#
# This pulls in _all_ .c and .h files in the src directory. Even files that has
# not been added to the cvs repo. Beware of junk entries!

$(srcdir)/$(PACKAGE).pot: $(POTFILES_REL)
	$(XGETTEXT) --default-domain=$(PACKAGE) \
		    --directory=$(top_srcdir) \
		    --add-comments --language=C \
		    --keyword=_ --keyword=N_ --keyword=n_:1,2 --keyword=N__ $(POTFILES_ABS) \
	&& test ! -f $(PACKAGE).po \
	   || ( rm -f $(srcdir)/$(PACKAGE).pot \
		&& mv $(PACKAGE).po $(srcdir)/$(PACKAGE).pot )


### Updating po and gmo files
#
# Update by merging all .po files with the .po template master file.
#
# It is possible to update only a single .po file by setting the PO variable to
# either <lang> or <lang>.po when calling make. Example: make update-po PO=is

update-po: Makefile $(srcdir)/$(PACKAGE).pot
	@cd $(srcdir); \
	if test -n "$(PO)"; then \
	  lang=`echo $(PO) | sed 's/\.po//'`; \
	  catalogs=$$lang.gmo; \
	fi; \
	if test -z "$$catalogs"; then catalogs='$(GMOFILES)'; fi; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\.gmo$$//'`; \
	  echo -n "$$lang: "; \
	  if $(MSGMERGE) $$lang.po $(PACKAGE).pot -o $$lang.new.po; then \
	    mv -f $$lang.new.po $$lang.po; \
	  else \
	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$lang.new.po; \
	  fi; \
	done
	$(MAKE) update-gmo

update-gmo: Makefile $(GMOFILES)
	@:


### Check single or all po files
#

check-po:
	@if test -n "$(PO)"; then \
	  lang=`echo $(PO) | sed 's/\.po//'`; \
	  catalogs=$$lang.gmo; \
	fi; \
	if test -z "$$catalogs"; then catalogs='$(GMOFILES)'; fi; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\.gmo$$//'`; \
	  echo -n "$$lang: "; \
	  $(GMSGFMT) --check --check-accelerators=~ --verbose --statistics -o /dev/null $$lang.po; \
	done

### Installation and distribution
#
# Use *-l targets to place and remove .gmo files.

install-l: install-l-$(CONFIG_NLS)
install-l-no:
install-l-yes:
	$(mkinstalldirs) $(DESTDIR)$(localedir)
	@catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\.gmo$$//'`; \
	  dir=$(localedir)/$$lang/LC_MESSAGES; \
	  $(mkinstalldirs) $(DESTDIR)$$dir; \
	  if test -r $(srcdir)/$$cat; then \
	    $(INSTALL_DATA) $(srcdir)/$$cat $(DESTDIR)$$dir/$(PACKAGE).mo; \
	    echo "installing $$cat as $(DESTDIR)$$dir/$(PACKAGE).mo"; \
	  else \
	    echo "Error: Cannot find $$cat"; \
	  fi; \
	done

# Steal the `dist' target so that .po and .gmo files will be properly updated
# then the dependence on `distdir' will take care of copying.
dist: update-po distdir

clean:
	rm -f $(PACKAGE).po *.new.po

-include ../Makefile.lib
