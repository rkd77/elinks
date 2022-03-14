#INCLUDES += $(X_CFLAGS)

if conf_data.get('CONFIG_OS_BEOS')
	subdir('beos')
endif
if conf_data.get('CONFIG_OS_OS2')
	subdir('os2')
endif
if conf_data.get('CONFIG_OS_RISCOS')
	subdir('riscos')
endif
if conf_data.get('CONFIG_OS_UNIX')
	subdir('unix')
endif
if conf_data.get('CONFIG_OS_WIN32')
	subdir('win32')
endif

srcs += files('getifaddrs.c', 'newwin.c', 'osdep.c', 'signals.c', 'stub.c', 'sysname.c')
