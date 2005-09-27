path_to_top=..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_BOOKMARKS),yes)
bookmarksdir = bookmarks
endif

ifeq ($(CONFIG_COOKIES),yes)
cookiesdir = cookies
endif

ifeq ($(CONFIG_FORMHIST),yes)
formhistdir = formhist
endif

ifeq ($(CONFIG_GLOBHIST),yes)
globhistdir = globhist
endif

ifeq ($(CONFIG_ECMASCRIPT),yes)
ecmascriptdir = ecmascript
endif

ifeq ($(CONFIG_SCRIPTING),yes)
scriptingdir = scripting
endif

SUBDIRS = \
 bfu \
 $(bookmarksdir) \
 cache \
 config \
 $(cookiesdir) \
 dialogs \
 document \
 $(ecmascriptdir) \
 encoding \
 $(formhistdir) \
 $(globhistdir) \
 intl \
 main \
 mime \
 network \
 osdep \
 protocol \
 $(scriptingdir) \
 session \
 terminal \
 util \
 viewer

# Get the GIT HEAD ID if possible
ifdef CG_COMMIT_ID
BUILD_ID=$(shell $(CG_COMMIT_ID) 2> /dev/null)
endif
INCLUDES += -DBUILD_ID="\"$(BUILD_ID)\""

OBJS = vernum.o
vernum.o: FORCE
FORCE:

all-local: elinks
elinks: lib.o
	$(call cmd,link)

install-local:
	$(INSTALL_PROGRAM) elinks $(DESTDIR)$(bindir)

include $(path_to_top)/Makefile.lib

