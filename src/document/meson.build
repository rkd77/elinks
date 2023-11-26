if conf_data.get('CONFIG_CSS')
	subdir('css')
endif
if conf_data.get('CONFIG_DOM')
	subdir('dom')
endif
if conf_data.get('CONFIG_LIBDOM')
	subdir('libdom')
endif
subdir('gemini')
subdir('html')
subdir('plain')
srcs += files('docdata.c', 'document.cpp', 'format.c', 'forms.c', 'options.c', 'refresh.c', 'renderer.c')
