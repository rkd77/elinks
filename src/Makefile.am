## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.82 2004/09/21 22:37:37 jonas Exp $

include $(top_srcdir)/Makefile.base

EXTRA_DIST = README

if CONFIG_BOOKMARKS
bookmarksdir = bookmarks
bookmarkslib = $(bookmarksdir)/libbookmarks.a
endif

if CONFIG_COOKIES
cookiesdir = cookies
cookieslib = $(cookiesdir)/libcookies.a
endif

if CONFIG_FORMHIST
formhistdir = formhist
formhistlib = $(formhistdir)/libformhist.a
endif

if CONFIG_GLOBHIST
globhistdir = globhist
globhistlib = $(globhistdir)/libglobhist.a
endif

if CONFIG_ECMASCRIPT
ecmascriptdir = ecmascript
ecmascriptlib = $(ecmascriptdir)/libecmascript.a
endif

if CONFIG_SCRIPTING
scriptingdir = scripting
scriptinglib = $(scriptingdir)/libscripting.a
endif

if CONFIG_SSL
ssldir = ssl
ssllib = $(ssldir)/libssl.a
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
 lowlevel \
 mime \
 modules \
 osdep \
 protocol \
 sched \
 $(scriptingdir) \
 $(ssldir) \
 terminal \
 util \
 viewer

bin_PROGRAMS = elinks
elinks_SOURCES = \
 elinks.h \
 main.c \
 main.h \
 setup.h \
 vernum.c \
 vernum.h

if COMMIT_ID
BUILD_ID = -DBUILD_ID="\"$$($(COMMIT_ID) 2> /dev/null)\""
endif

INCLUDES += $(BUILD_ID)

vernum.o: FORCE

FORCE:

# Order of this is purely magic and random, so that ld doesn't fail for some
# strange reason. Try to swap randomly when ld will start to complain about
# mysteriously unresolved symbols.
elinks_DEPENDENCIES = \
 @INTLLIBS@ \
 modules/libmodules.a \
 $(cookieslib) \
 viewer/text/libviewer_text.a \
 viewer/dump/libviewer_dump.a \
 cache/libcache.a \
 document/libdocument.a \
 intl/libintl.a \
 sched/libsched.a \
 lowlevel/liblowlevel.a \
 terminal/libterminal.a \
 $(scriptinglib) \
 osdep/libosdep.a \
 protocol/libprotocol.a \
 $(ssllib) \
 $(bookmarkslib) \
 $(formhistlib) \
 $(globhistlib) \
 $(ecmascriptlib) \
 config/libconfig.a \
 dialogs/libdialogs.a \
 mime/libmime.a \
 bfu/libbfu.a \
 encoding/libencoding.a \
 util/libutil.a

elinks_LDADD = $(elinks_DEPENDENCIES)
