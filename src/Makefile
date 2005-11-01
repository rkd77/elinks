top_builddir=..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_BOOKMARKS)	+= bookmarks
SUBDIRS-$(CONFIG_COOKIES)	+= cookies
SUBDIRS-$(CONFIG_FORMHIST)	+= formhist
SUBDIRS-$(CONFIG_GLOBHIST)	+= globhist
SUBDIRS-$(CONFIG_ECMASCRIPT)	+= ecmascript
SUBDIRS-$(CONFIG_SCRIPTING)	+= scripting

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
ifdef CG_COMMIT_ID
BUILD_ID=$(shell GIT_DIR=$(top_srcdir)/.git $(CG_COMMIT_ID) 2> /dev/null)
endif
INCLUDES += -DBUILD_ID="\"$(BUILD_ID)\""

OBJS = vernum.o
vernum.o: FORCE
FORCE:

elinks: lib.o
	$(call cmd,link)

PROGS = elinks

include $(top_srcdir)/Makefile.lib
