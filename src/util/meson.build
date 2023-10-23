subdir('qs_parse')

if not conf_data.get('CONFIG_SMALL')
	srcs += files('fastfind.c')
endif
if conf_data.get('CONFIG_CSS') or conf_data.get('CONFIG_BITTORRENT')
	srcs += files('scanner.c')
endif
if conf_data.get('CONFIG_DEBUG')
	srcs += files('memcount.cpp', 'memdebug.c')
endif
if conf_data.get('CONFIG_DOM')
	srcs += files('scanner.c')
endif
if conf_data.get('CONFIG_BITTORRENT')
	if conf_data.get('CONFIG_OWN_LIBC') or not conf_data.get('CONFIG_OPENSSL')
		srcs += files('sha1.c')
	endif
endif

srcs += files('base64.c', 'color.c', 'conv.c', 'env.c', 'error.c', 'file.c', 'hash.c',
	'md5.c', 'memlist.c', 'memory.c', 'random.c', 'secsave.c', 'snprintf.c', 'string.c', 'time.c')
