#localedir = $(datadir)/locale
#builddir = $(top_builddir)/src/intl/gettext
#CFLAGS += $(GETTEXT_CFLAGS)

srcs += files('bindtextdom.c', 'dcgettext.c', 'dcigettext.c', 'dcngettext.c', 'dgettext.c', 'dngettext.c', 'explodename.c',
	'finddomain.c', 'gettext.c', 'intl-compat.c', 'l10nflist.c', 'libintl.c', 'loadmsgcat.c', 'localcharset.c',
	'localealias.c', 'ngettext.c', 'plural.c', 'textdomain.c')


# $(builddir)/charset.alias: $(srcdir)config.charset
##$(builddir)/charset.alias: $(srcdir)config.charset
##	$(SHELL) $(srcdir)config.charset '$(host)' > $@.new
##	mv $@.new $@

# FIXME: Building plural.c from plural.y on the fly doesn't work

##YACC = $(INTLBISON) -y -d
##YFLAGS = --name-prefix=gettext__
##plural.c: plural.y
##.SUFFIXES: .y .c
##.y.c:
##	$(YACC) $(YFLAGS) --output $@ $<
##	rm -f $*.h

##install-local: $(builddir)/charset.alias all
##	@if test '$(CONFIG_NLS)' = yes; then \
##	  temp=$(DESTDIR)$(libdir)/t-charset.alias; \
##	  dest=$(DESTDIR)$(libdir)/charset.alias; \
##	  if test -f $(DESTDIR)$(libdir)/charset.alias; then \
##	    orig=$(DESTDIR)$(libdir)/charset.alias; \
##	    sed -f ref-add.sed $$orig > $$temp; \
##	    $(INSTALL_DATA) $$temp $$dest; \
##	    rm -f $$temp; \
##	  else \
##	    if test $(GLIBC21) = no; then \
##	      $(MKINSTALLDIRS) $(DESTDIR)$(libdir); \
##	      orig=charset.alias; \
##	      sed -f ref-add.sed $$orig > $$temp; \
##	      $(INSTALL_DATA) $$temp $$dest; \
##	      rm -f $$temp; \
##	    fi; \
##	  fi; \
##	  $(MKINSTALLDIRS) $(DESTDIR)$(localedir); \
##	  test -f $(DESTDIR)$(localedir)/locale.alias \
##	    && orig=$(DESTDIR)$(localedir)/locale.alias \
##	    || orig=$(srcdir)locale.alias; \
##	  temp=$(DESTDIR)$(localedir)/t-locale.alias; \
##	  dest=$(DESTDIR)$(localedir)/locale.alias; \
##	  sed -f ref-add.sed $$orig > $$temp; \
##	  $(INSTALL_DATA) $$temp $$dest; \
##	  rm -f $$temp; \
##	else \
##	  : ; \
##	fi

##uninstall-local:
##	@if test '$(CONFIG_NLS)' = yes; then \
##	  if test -f $(DESTDIR)$(libdir)/charset.alias; then \
##	    temp=$(DESTDIR)$(libdir)/t-charset.alias; \
##	    dest=$(DESTDIR)$(libdir)/charset.alias; \
##	    sed -f ref-del.sed $$dest > $$temp; \
##	    if grep '^# Packages using this file: $$' $$temp > /dev/null; then \
##	      rm -f $$dest; \
##	    else \
##	      $(INSTALL_DATA) $$temp $$dest; \
##	    fi; \
##	    rm -f $$temp; \
##	  fi; \
##	  if test -f $(DESTDIR)$(localedir)/locale.alias; then \
##	    temp=$(DESTDIR)$(localedir)/t-locale.alias; \
##	    dest=$(DESTDIR)$(localedir)/locale.alias; \
##	    sed -f ref-del.sed $$dest > $$temp; \
##	    if grep '^# Packages using this file: $$' $$temp > /dev/null; then \
##	      rm -f $$dest; \
##	    else \
##	      $(INSTALL_DATA) $$temp $$dest; \
##	    fi; \
##	    rm -f $$temp; \
##	  fi; \
##	else \
##	  : ; \
##	fi
