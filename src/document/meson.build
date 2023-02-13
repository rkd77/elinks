if conf_data.get('CONFIG_CSS') and conf_data.get('CONFIG_DOM')
	subdir('css')
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
