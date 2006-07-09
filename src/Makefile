top_builddir=..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_BOOKMARKS)	+= bookmarks
SUBDIRS-$(CONFIG_COOKIES)	+= cookies
SUBDIRS-$(CONFIG_FORMHIST)	+= formhist
SUBDIRS-$(CONFIG_GLOBHIST)	+= globhist
SUBDIRS-$(CONFIG_ECMASCRIPT)	+= ecmascript
SUBDIRS-$(CONFIG_SCRIPTING)	+= scripting
SUBDIRS-$(CONFIG_DOM)		+= dom

SUBDIRS = \
 bfu \
 cache \
 config \
 dialogs \
 document \
 encoding \
 intl \
 main \
 mime \
 network \
 osdep \
 protocol \
 session \
 terminal \
 util \
 viewer

# Get the GIT HEAD ID if possible
ifdef GIT
GITDESC  = $(shell $(GIT) --git-dir=$(top_srcdir)/.git describe 2>/dev/null)
WTDIRTY  = $(shell $(GIT) --git-dir=$(top_srcdir)/.git diff-index HEAD 2>/dev/null)
BUILD_ID = $(subst elinks-,,$(GITDESC))$(if $(WTDIRTY),-dirty)
endif
INCLUDES += -DBUILD_ID="\"$(BUILD_ID)\""

vernum.o: $(LIB_O_NAME)

elinks: $(LIB_O_NAME) vernum.o
	$(call cmd,link)

PROGS = elinks

include $(top_srcdir)/Makefile.lib
