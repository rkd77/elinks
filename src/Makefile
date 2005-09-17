path_to_top=..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_BOOKMARKS),yes)
bookmarksdir = bookmarks
bookmarkslib = $(bookmarksdir)/libbookmarks.a
endif

ifeq ($(CONFIG_COOKIES),yes)
cookiesdir = cookies
cookieslib = $(cookiesdir)/libcookies.a
endif

ifeq ($(CONFIG_FORMHIST),yes)
formhistdir = formhist
formhistlib = $(formhistdir)/libformhist.a
endif

ifeq ($(CONFIG_GLOBHIST),yes)
globhistdir = globhist
globhistlib = $(globhistdir)/libglobhist.a
endif

ifeq ($(CONFIG_ECMASCRIPT),yes)
ecmascriptdir = ecmascript
ecmascriptlib = $(ecmascriptdir)/libecmascript.a
endif

ifeq ($(CONFIG_SCRIPTING),yes)
scriptingdir = scripting
scriptinglib = $(scriptingdir)/libscripting.a
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
 main/libmain.a \
 $(cookieslib) \
 viewer/libviewer.a \
 cache/libcache.a \
 document/libdocument.a \
 intl/libintl.a \
 session/libsession.a \
 network/libnetwork.a \
 terminal/libterminal.a \
 $(scriptinglib) \
 osdep/libosdep.a \
 protocol/libprotocol.a \
 $(bookmarkslib) \
 $(formhistlib) \
 $(globhistlib) \
 $(ecmascriptlib) \
 config/libconfig.a \
 dialogs/libdialogs.a \
 mime/libmime.a \
 bfu/libbfu.a \
 encoding/libencoding.a \
 $(INTLLIBS) \
 util/libutil.a

# Get the GIT HEAD ID if possible
ifdef COMMIT_ID
INCLUDES += -DBUILD_ID="\"$$($(COMMIT_ID) 2> /dev/null)\""
endif

OBJS = vernum.o
vernum.o: FORCE
FORCE:

all-l: elinks
elinks: $(OBJS) $(ELINKSLIBS)
	$(call cmd,link)

include $(path_to_top)/Makefile.lib
