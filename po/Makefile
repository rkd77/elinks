top_builddir=..
include $(top_builddir)/Makefile.config

# We prefer the capitalized name for Project-Id-Version.
ifeq ($(PACKAGE), elinks)
PRETTY_PACKAGE = ELinks
else
PRETTY_PACKAGE = $(PACKAGE)
endif

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

$(POTFILES_ABS_LIST): $(POTFILES_REL)
	@( cd $(top_srcdir); \
	  find src/ -type f -name '*.[ch]' -o -name options.inc -o -name 'actions-*.inc' | sort ) \
	 > $(POTFILES_ABS_LIST)

# xgettext --flag requires GNU gettext 0.13 or later;
# --msgid-bugs-address requires 0.12 or later.
$(srcdir)$(PACKAGE).pot: $(POTFILES_ABS_LIST) $(srcdir)perl/msgaccel-prepare
	$(XGETTEXT) --default-domain=$(PACKAGE) \
		    --directory=$(top_srcdir) \
		    --msgid-bugs-address=elinks-users@linuxfromscratch.org \
		    --add-comments --language=C \
		    --keyword=_ --keyword=N_ --keyword=n_:1,2 --keyword=N__ \
		    --flag=msg_text:2:c-format --flag=die:1:c-format \
		    --flag=secure_fprintf:2:c-format \
		    --flag=DBG:1:c-format --flag=elinks_debug:1:c-format \
		    --flag=WDBG:1:c-format --flag=elinks_wdebug:1:c-format \
		    --flag=ERROR:1:c-format --flag=elinks_error:1:c-format \
		    --flag=INTERNAL:1:c-format --flag=elinks_internal:1:c-format \
		    --flag=usrerror:1:c-format --flag=elinks_log:4:c-format \
		    --flag=LOG_ERR:1:c-format --flag=LOG_WARN:1:c-format \
		    --flag=LOG_INFO:1:c-format --flag=LOG_DBG:1:c-format \
		    --flag=assertm:2:c-format --flag=elinks_assertm:2:c-format \
		    --flag=add_format_to_string:2:c-format \
		    --flag=elinks_vsnprintf:3:c-format --flag=elinks_snprintf:3:c-format \
		    --flag=elinks_vasprintf:2:c-format --flag=elinks_asprintf:2:c-format \
		    --flag=vasprintfa:1:c-format --flag=asprintfa:1:c-format \
		    --flag=_:1:pass-c-format --flag=N_:1:pass-c-format \
		    --flag=n_:1:pass-c-format --flag=n_:2:pass-c-format \
		    --flag=N__:1:pass-c-format \
		    -f $(POTFILES_ABS_LIST)
	test -f $(PACKAGE).po
	$(PERL) -I"$(srcdir)perl" $(srcdir)perl/msgaccel-prepare -S"$(top_srcdir)" $(PACKAGE).po
# GNU gettext 0.17 supports xgettext --package-version but is GPLv3+.
# This rule already requires Perl so use that instead.
	$(PERL) -pi -e 's/(^"Project-Id-Version: )PACKAGE VERSION/$$1$(PRETTY_PACKAGE) $(VERSION)/i and $$found=1 unless $$found' $(PACKAGE).po
	mv -f $(PACKAGE).po $(srcdir)$(PACKAGE).pot


### Updating po and gmo files
#
# Update by merging all .po files with the .po template master file.
#
# It is possible to update only a single .po file by setting the PO variable to
# either <lang> or <lang>.po when calling make. Example: make update-po PO=is

update-po: Makefile $(srcdir)$(PACKAGE).pot
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
	  $(PERL) -I"$(srcdir)perl" $(srcdir)perl/msgaccel-check $(srcdir)$(lang).po; \
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

uninstall-local:
	@$(foreach lang,$(basename $(if $(strip $(PO)),$(PO),$(CATALOGS))), \
		$(call ncmd,uninstall,$(PACKAGE).mo,$(DESTDIR)$(localedir)/$(lang)/LC_MESSAGES); \
	)

clean-local:
	@rm -f $(PACKAGE).po *.new.po $(POTFILES_ABS_LIST)

include $(top_srcdir)/Makefile.lib
