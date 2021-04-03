if conf_data.get('CONFIG_SCRIPTING_GUILE')
	subdir('guile')
endif
if conf_data.get('CONFIG_SCRIPTING_LUA')
	subdir('lua')
endif
if conf_data.get('CONFIG_SCRIPTING_PERL')
	subdir('perl')
endif
if conf_data.get('CONFIG_SCRIPTING_PYTHON')
	subdir('python')
endif
if conf_data.get('CONFIG_SCRIPTING_RUBY')
	subdir('ruby')
endif
if conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY')
	subdir('smjs')
endif
srcs += files('scripting.c')
