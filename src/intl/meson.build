if conf_data.get('CONFIG_GETTEXT')
    srcs += files('libintl.c')
elif conf_data.get('CONFIG_NLS')
    subdir('gettext')
endif

srcs += files('charsets.c')

if conf_data.get('CONFIG_UTF8')
    srcs += files('width.c')
endif
