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

extra_args = ['-DHAVE_CONFIG_H', '-fno-strict-aliasing', '-Wno-address']

if compiler.has_argument('-Wno-builtin-declaration-mismatch')
	extra_args += ['-Wno-builtin-declaration-mismatch']
endif

extra_args += extracflags

extra_cpp_args = extra_args
extra_cpp_args += extracppflags

elinks = executable(
  'elinks', srcs,
  include_directories: incs,
  dependencies: deps,
  c_args: extra_args,
  cpp_args: extra_cpp_args,
  install: true
)
