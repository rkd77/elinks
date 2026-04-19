#INCLUDES += $(X_CFLAGS)

if conf_data.get('CONFIG_OS_BEOS')
	subdir('beos')
endif
if conf_data.get('CONFIG_OS_DOS')
	subdir('dos')
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

srcs += files('newwin.c', 'osdep.c', 'signals.c', 'stub.c', 'sysname.c')

if conf_data.get('HAVE_GETIFADDRS') != 1
	srcs += files('getifaddrs.c')
endif