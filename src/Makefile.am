## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.87 2005/06/14 12:25:19 jonas Exp $

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

bin_PROGRAMS = elinks
elinks_SOURCES = \
 elinks.h \
 setup.h \
 vernum.c \
 vernum.h

vernum.o: FORCE

FORCE:

# Order of this is purely magic and random, so that ld doesn't fail for some
# strange reason. Try to swap randomly when ld will start to complain about
# mysteriously unresolved symbols.
elinks_DEPENDENCIES = \
 main/libmain.a \
 @INTLLIBS@ \
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
 util/libutil.a

elinks_LDADD = $(elinks_DEPENDENCIES)
