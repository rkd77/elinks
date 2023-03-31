#INCLUDES += $(SPIDERMONKEY_CFLAGS)
if conf_data.get('CONFIG_ECMASCRIPT_SMJS')
	subdir('spidermonkey')
	srcs += files('css2xpath.cpp', 'ecmascript.cpp', 'localstorage-db.cpp', 'spidermonkey.cpp', 'timer.cpp')
endif

if conf_data.get('CONFIG_ECMASCRIPT_SMJS')
	CONFIG_ANY_SPIDERMONKEY = true
elif conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY')
	CONFIG_ANY_SPIDERMONKEY = true
else
	CONFIG_ANY_SPIDERMONKEY = false
endif

if CONFIG_ANY_SPIDERMONKEY
	srcs += files('spidermonkey-shared.cpp')
endif

if conf_data.get('CONFIG_MUJS')
	subdir('mujs')
	srcs += files('css2xpath.cpp', 'ecmascript.cpp', 'localstorage-db.cpp', 'mujs.cpp', 'timer.cpp')
endif

if conf_data.get('CONFIG_QUICKJS')
	subdir('quickjs')
	srcs += files('css2xpath.cpp', 'ecmascript.cpp', 'localstorage-db.cpp', 'quickjs.cpp', 'timer.cpp')
endif

if conf_data.get('CONFIG_LIBDOM')
	subdir('libdom')
endif
