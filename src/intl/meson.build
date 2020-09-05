if conf_data.get('CONFIG_NLS') == 1
	subdir('gettext')
endif
srcs += files('charsets.c')

if conf_data.get('CONFIG_UTF8')
	srcs += files('width.c')
endif
