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

srcs += files('vernum.c')

incs = include_directories('..', '.')

extra_args = ['-DHAVE_CONFIG_H', '-fno-strict-aliasing', '-Wno-builtin-declaration-mismatch', '-Wno-address'] + extracflags

elinks = executable(
  'elinks', srcs,
  include_directories: incs,
  dependencies: deps,
  c_args: extra_args,
  install: true
)
