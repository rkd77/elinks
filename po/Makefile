path_to_top=..
include $(path_to_top)/Makefile.config

# Where to install the catalog files.
localedir = $(datadir)/locale

# These are for some reason not added magically to the Makefile.
MSGMERGE = msgmerge

POTFILES_REL = $(shell find $(top_srcdir)/src/ -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort)
POTFILES_ABS = $(shell cd $(top_srcdir); find src/ -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort)


SUFFIXES = .po .gmo .mo
.SUFFIXES: .gmo .mo .po

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

all-local: $(CATALOGS)


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
	@cd $(srcdir)
	@$(foreach C,$(basename $(if $(strip $(PO)),$(PO),$(GMOFILES))), \
	  echo -n "$(C): "; \
	  if $(MSGMERGE) $(C).po $(PACKAGE).pot -o $(C).new.po; then \
	    mv -f $(C).new.po $(C).po; \
	  else \
	    echo "msgmerge failed!"; \
	    rm -f $(C).new.po; \
	  fi; \
	)
	$(MAKE) update-gmo

update-gmo: Makefile $(GMOFILES)
	@:


### Check single or all po files
#

check-po:
	@$(foreach C,$(basename $(if $(strip $(PO)),$(PO),$(GMOFILES))), \
	  echo -n "$(C): "; \
	  $(GMSGFMT) --check --check-accelerators=~ --verbose --statistics -o /dev/null $(C).po; \
	)

### Installation and distribution
#
# Use *-local targets to place and remove .gmo files.

install-local: install-local-$(CONFIG_NLS)
install-local-no:
install-local-yes:
	$(MKINSTALLDIRS) $(DESTDIR)$(localedir)
	@$(foreach C,$(basename $(CATALOGS)), \
		$(MKINSTALLDIRS) $(DESTDIR)$(localedir)/$(C)/LC_MESSAGES; \
		$(call ncmd,installdata,$(C).gmo,$(DESTDIR)$(localedir)/$(C)/LC_MESSAGES/$(PACKAGE).mo); \
	)

# Steal the `dist' target so that .po and .gmo files will be properly updated
# then the dependence on `distdir' will take care of copying.
dist: update-po distdir

clean-local:
	rm -f $(PACKAGE).po *.new.po

include $(path_to_top)/Makefile.lib
