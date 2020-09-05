if conf_data.get('CONFIG_BOOKMARKS')
	subdir('bookmarks')
endif
if conf_data.get('CONFIG_COOKIES')
	subdir('cookies')
endif
if conf_data.get('CONFIG_DOM')
	subdir('dom')
endif
if conf_data.get('CONFIG_ECMASCRIPT')
	subdir('ecmascript')
endif
if conf_data.get('CONFIG_FORMHIST')
	subdir('formhist')
endif
if conf_data.get('CONFIG_GLOBHIST')
	subdir('globhist')
endif
if conf_data.get('CONFIG_SCRIPTING')
	subdir('scripting')
endif

subdir('bfu')
subdir('cache')
subdir('config')
subdir('dialogs')
subdir('document')
subdir('encoding')
subdir('intl')
subdir('main')
subdir('mime')
subdir('network')
subdir('osdep')
subdir('protocol')
subdir('session')
subdir('terminal')
subdir('util')
subdir('viewer')

# Get the GIT HEAD ID if possible
##ifdef GIT
##GITDESC  = $(shell $(GIT) --git-dir=$(top_srcdir)/.git rev-parse HEAD 2>/dev/null)
##WTDIRTY  = $(shell cd "$(top_srcdir)" && $(GIT) diff-index HEAD 2>/dev/null)
##BUILD_ID = $(subst elinks-,,$(GITDESC))$(if $(WTDIRTY),-dirty)
##endif
##INCLUDES += -DBUILD_ID="\"$(BUILD_ID)\""

srcs += files('vernum.c')

incs = include_directories('..', '.', '/usr/include/luajit-2.0')

extra_args = ['-DHAVE_CONFIG_H', '-fno-strict-aliasing', '-Wno-pointer-sign', '-Wno-builtin-declaration-mismatch', '-Wno-address']


##deps= [zdeps, ltredeps, eventdeps, ssldeps, brotlideps, zstddeps, idndeps, x11deps, bz2deps, mozjsdeps, luadeps, xmldeps, libgpm, domdeps]

#libs = ['-ltre', '-Wl,--as-needed', '-Wl,--no-copy-dt-needed-entries',
#	'-Wl,-z,relro', '-Wl,-z,combreloc', '-lX11', '-levent', '-lssl', '-lcrypto', '-ldl', '-lgpm', '-lz', '-lbz2',
#	'-lbrotlidec', '-lzstd', '-lidn', '-lexpat', '-lmozjs-17.0']

elinks = executable(
  'elinks', srcs,
  include_directories: incs,
  dependencies: deps,
  c_args : extra_args
  )

##elinks$(EXEEXT): $(LIB_O_NAME) vernum.o
##	$(call cmd,link)

# Place the TAGS file in the source directory so that, if the same
# source is built for different configurations in different build
# directories, one doesn't have to remember which of those build
# directories has the most recent TAGS.
##TAGS:
##	cd $(firstword $(srcdir) .) \
##	&& find . \( -name "*.[ch]" -o -name "*.inc" \) -print \
##	| etags --regex='{c}/\(?:static \)?INIT_LIST_OF([^,]*,[ \t]*\([[:alnum:]_]+\))/\1/' \
##		--regex='{c}/struct_hierbox_browser(\n[ \t]*\([[:alnum:]_]+\),/\1/m' \
##		--regex='{c}/^ACTION_(\([[:alnum:]_]+\),[^,]*,[ \t]*\([[:alnum:]_]+\),/ACT_\1_\2/' \
##		--language=c -

##tags:
##	cd $(firstword $(srcdir) .) \
##	&& find . \( -name "*.[ch]" -o -name "*.inc" \) -print \
##	| ctags -L -

##.PHONY: TAGS tags

##PROGS = elinks$(EXEEXT)
##CLEAN = vernum.o TAGS tags
