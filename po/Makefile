top_builddir=..
include $(top_builddir)/Makefile.config

# Where to install the catalog files.
localedir = $(datadir)/locale

# These are for some reason not added magically to the Makefile.
MSGMERGE = msgmerge
PERL = perl

# Name of the file containing the list of files to translate (used by
# xgettext) 
POTFILES_ABS_LIST = potfiles.list

POTFILES_REL = $(shell find $(top_srcdir)/src/ -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort)

quiet_cmd_gmsgfmt = ' [$(PO_COLOR)GMSGFMT$(END_COLOR)]   $(RELPATH)$(@)'
      cmd_gmsgfmt = rm -f -- "$@" && $(GMSGFMT) --statistics -o "$@" -- "$<"


# Distributed elinks-*.tar.gz packages include po/*.gmo files, so that
# users can install ELinks even if they don't have a msgfmt program.
# However, if srcdir != builddir, then this Makefile ignores the *.gmo
# files in the srcdir and builds new ones in the builddir.
%.gmo: $(srcdir)%.po
	$(call ecmd,gmsgfmt)


### The default rule
#
# Note: Target `all-local' must not depend on target '$(srcdir)$(PACKAGE).pot',
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
# not been added to the git repo. Beware of junk entries!

$(srcdir)$(POTFILES_ABS_LIST): $(POTFILES_REL)
	@( cd $(top_srcdir); \
	  find src/ -type f -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort ) \
	 > $(srcdir)$(POTFILES_ABS_LIST)

$(srcdir)$(PACKAGE).pot: $(srcdir)$(POTFILES_ABS_LIST) $(srcdir)perl/gather-accelerator-contexts.pl
	$(XGETTEXT) --default-domain=$(PACKAGE) \
		    --directory=$(top_srcdir) \
		    --add-comments --language=C \
		    --keyword=_ --keyword=N_ --keyword=n_:1,2 --keyword=N__ -f $(srcdir)$(POTFILES_ABS_LIST) \
	&& test -f $(PACKAGE).po \
	&& $(PERL) -I"$(srcdir)perl" $(srcdir)perl/gather-accelerator-contexts.pl -S"$(top_srcdir)" $(PACKAGE).po \
	&& mv -f $(PACKAGE).po $(srcdir)$(PACKAGE).pot


### Updating po and gmo files
#
# Update by merging all .po files with the .po template master file.
#
# It is possible to update only a single .po file by setting the PO variable to
# either <lang> or <lang>.po when calling make. Example: make update-po PO=is

update-po: Makefile $(srcdir)$(PACKAGE).pot
	@test -z "$(srcdir)" || cd $(srcdir)
	@$(foreach lang,$(basename $(if $(strip $(PO)),$(PO),$(GMOFILES))), \
	  echo -n "$(lang): "; \
	  if $(MSGMERGE) $(srcdir)$(lang).po $(srcdir)$(PACKAGE).pot -o $(lang).new.po; then \
	    mv -f $(lang).new.po $(srcdir)$(lang).po; \
	  else \
	    echo "msgmerge failed!"; \
	    rm -f $(lang).new.po; \
	  fi; \
	)
	$(MAKE) update-gmo

update-gmo: Makefile $(GMOFILES)
	@:


### Check single or all po files
#

check-po:
	@-$(foreach lang,$(basename $(if $(strip $(PO)),$(PO),$(GMOFILES))), \
	  echo -n "$(lang): "; \
	  $(GMSGFMT) --check --check-accelerators="~" --verbose --statistics -o /dev/null $(srcdir)$(lang).po; \
	  $(PERL) -I"$(srcdir)perl" $(srcdir)perl/check-accelerator-contexts.pl $(srcdir)$(lang).po; \
	)

### Installation and distribution
#
# Use *-local targets to place and remove .gmo files.

install-local: all-local
	$(MKINSTALLDIRS) $(DESTDIR)$(localedir)
	@$(foreach lang,$(basename $(if $(strip $(PO)),$(PO),$(CATALOGS))), \
		$(MKINSTALLDIRS) $(DESTDIR)$(localedir)/$(lang)/LC_MESSAGES; \
		$(call ncmd,installdata,$(lang).gmo,$(DESTDIR)$(localedir)/$(lang)/LC_MESSAGES/$(PACKAGE).mo); \
	)

clean-local:
	@rm -f $(PACKAGE).po *.new.po $(srcdir)$(POTFILES_ABS_LIST)

include $(top_srcdir)/Makefile.lib
