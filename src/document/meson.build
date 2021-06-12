if conf_data.get('CONFIG_CSS') == 1
	subdir('css')
endif
if conf_data.get('CONFIG_DOM')
	subdir('dom')
endif
if conf_data.get('CONFIG_LIBDOM')
	subdir('libdom')
endif
subdir('html')
subdir('plain')
srcs += files('docdata.c', 'document.c', 'format.c', 'forms.c', 'options.c', 'refresh.c', 'renderer.c')
