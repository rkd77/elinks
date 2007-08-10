top_builddir=..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_BOOKMARKS)	+= bookmarks
SUBDIRS-$(CONFIG_COOKIES)	+= cookies
SUBDIRS-$(CONFIG_DOM)		+= dom
SUBDIRS-$(CONFIG_ECMASCRIPT)	+= ecmascript
SUBDIRS-$(CONFIG_FORMHIST)	+= formhist
SUBDIRS-$(CONFIG_GLOBHIST)	+= globhist
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
else
ifdef CG_OBJECT_ID
BUILD_ID=$(shell GIT_DIR=$(top_srcdir)/.git $(CG_OBJECT_ID) -c 2> /dev/null)
endif
endif
INCLUDES += -DBUILD_ID="\"$(BUILD_ID)\""

vernum.o: $(LIB_O_NAME)

elinks: $(LIB_O_NAME) vernum.o
	$(call cmd,link)

# Place the TAGS file in the source directory so that, if the same
# source is built for different configurations in different build
# directories, one doesn't have to remember which of those build
# directories has the most recent TAGS.
TAGS:
	cd $(firstword $(srcdir) .) \
	&& find . \( -name "*.[ch]" -o -name "*.inc" \) -print \
	| etags --regex='{c}/\(?:static \)?INIT_LIST_OF([^,]*,[ \t]*\([[:alnum:]_]+\))/\1/' \
		--regex='{c}/struct_hierbox_browser(\n[ \t]*\([[:alnum:]_]+\),/\1/m' \
		--regex='{c}/^ACTION_(\([[:alnum:]_]+\),[^,]*,[ \t]*\([[:alnum:]_]+\),/ACT_\1_\2/' \
		--language=c -

tags:
	cd $(firstword $(srcdir) .) \
	&& find . \( -name "*.[ch]" -o -name "*.inc" \) -print \
	| ctags -L -

.PHONY: TAGS tags

PROGS = elinks
CLEAN = vernum.o TAGS tags

include $(top_srcdir)/Makefile.lib
