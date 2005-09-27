path_to_top=..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_BOOKMARKS),yes)
bookmarksdir = bookmarks
bookmarkslib = $(bookmarksdir)/lib.o
endif

ifeq ($(CONFIG_COOKIES),yes)
cookiesdir = cookies
cookieslib = $(cookiesdir)/lib.o
endif

ifeq ($(CONFIG_FORMHIST),yes)
formhistdir = formhist
formhistlib = $(formhistdir)/lib.o
endif

ifeq ($(CONFIG_GLOBHIST),yes)
globhistdir = globhist
globhistlib = $(globhistdir)/lib.o
endif

ifeq ($(CONFIG_ECMASCRIPT),yes)
ecmascriptdir = ecmascript
ecmascriptlib = $(ecmascriptdir)/lib.o
endif

ifeq ($(CONFIG_SCRIPTING),yes)
scriptingdir = scripting
scriptinglib = $(scriptingdir)/lib.o
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

# Order of this is purely magic and random, so that ld doesn't fail for some
# strange reason. Try to swap randomly when ld will start to complain about
# mysteriously unresolved symbols.
ELINKSLIBS = \
 main/lib.o \
 $(cookieslib) \
 viewer/lib.o \
 cache/lib.o \
 document/lib.o \
 intl/lib.o \
 session/lib.o \
 network/lib.o \
 terminal/lib.o \
 $(scriptinglib) \
 osdep/lib.o \
 protocol/lib.o \
 $(bookmarkslib) \
 $(formhistlib) \
 $(globhistlib) \
 $(ecmascriptlib) \
 config/lib.o \
 dialogs/lib.o \
 mime/lib.o \
 bfu/lib.o \
 encoding/lib.o \
 intl/lib.o \
 util/lib.o

# Get the GIT HEAD ID if possible
ifdef CG_COMMIT_ID
BUILD_ID=$(shell $(CG_COMMIT_ID) 2> /dev/null)
endif
INCLUDES += -DBUILD_ID="\"$(BUILD_ID)\""

OBJS = vernum.o $(ELINKSLIBS)
vernum.o: FORCE
FORCE:

all-local: elinks
elinks: $(OBJS)
	$(call cmd,link)

install:
	$(INSTALL_PROGRAM) elinks $(DESTDIR)$(bindir)

include $(path_to_top)/Makefile.lib
