if conf_data.get('CONFIG_CSS')
	subdir('css')
endif
if conf_data.get('CONFIG_LIBCSS')
	subdir('css2')
endif
if conf_data.get('CONFIG_DOM')
	subdir('dom')
endif
subdir('gemini')
subdir('html')
subdir('plain')
if conf_data.get('CONFIG_XML')
	subdir('xml')
endif
srcs += files('docdata.c', 'document.cpp', 'format.c', 'forms.c', 'options.c', 'refresh.c', 'renderer.cpp')
