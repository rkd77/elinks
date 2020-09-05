#INCLUDES += $(SPIDERMONKEY_CFLAGS)
if conf_data.get('CONFIG_ECMASCRIPT_SMJS')
	subdir('spidermonkey')
	srcs += files('spidermonkey.c')
endif

if conf_data.get('CONFIG_ECMASCRIPT_SMJS')
	CONFIG_ANY_SPIDERMONKEY = true
elif conf_data.get('CONFIG_SCRIPTING_SPIDERMONKEY')
	CONFIG_ANY_SPIDERMONKEY = true
else
	CONFIG_ANY_SPIDERMONKEY = false
endif

if CONFIG_ANY_SPIDERMONKEY
	srcs += files('spidermonkey-shared.c')
endif

srcs += files('ecmascript.c')
