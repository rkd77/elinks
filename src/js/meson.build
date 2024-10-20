#INCLUDES += $(SPIDERMONKEY_CFLAGS)

if conf_data.get('CONFIG_ECMASCRIPT_SMJS') or conf_data.get('CONFIG_QUICKJS')
	xxd = find_program('xxd')
	run_command([xxd, '-i', 'fetch.js', 'fetch.h'], check: true)
endif

if conf_data.get('CONFIG_ECMASCRIPT_SMJS')
	srcs += files('ecmascript.cpp', 'ecmascript-c.cpp', 'localstorage-db.c', 'spidermonkey.cpp', 'timer.c')
	subdir('spidermonkey')
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
	srcs += files('ecmascript.c', 'ecmascript-c.c', 'localstorage-db.c', 'mujs.c', 'timer.c')
	subdir('mujs')
endif

if conf_data.get('CONFIG_QUICKJS')
	srcs += files('ecmascript.c', 'ecmascript-c.c', 'localstorage-db.c', 'quickjs.c', 'timer.c')
	subdir('quickjs')
endif

if conf_data.get('CONFIG_LIBDOM')
	subdir('libdom')
endif
