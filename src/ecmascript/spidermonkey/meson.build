#INCLUDES += $(SPIDERMONKEY_CFLAGS)

if conf_data.get('CONFIG_ECMASCRIPT_SMJS_HEARTBEAT')
	srcs += files('heartbeat.c')
endif
srcs += files('document.c', 'form.c', 'location.c', 'navigator.c', 'unibar.c', 'window.c')
