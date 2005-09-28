path_to_top=..
include $(path_to_top)/Makefile.config

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
	$(MKINSTALLDIRS) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) elinks $(DESTDIR)$(bindir)

include $(path_to_top)/Makefile.lib

