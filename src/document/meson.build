if conf_data.get('CONFIG_CSS')
	subdir('css')
endif
if conf_data.get('CONFIG_DOM')
	subdir('dom')
endif
subdir('html')
subdir('plain')
if conf_data.get('CONFIG_XML')
	subdir('xml')
endif
srcs += files('docdata.c', 'document.c', 'format.c', 'forms.c', 'options.c', 'refresh.c', 'renderer.c')
