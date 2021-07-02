if conf_data.get('CONFIG_BITTORRENT')
	subdir('bittorrent')
endif
if conf_data.get('CONFIG_FINGER')
	subdir('finger')
endif
if conf_data.get('CONFIG_FSP')
	subdir('fsp')
endif
if conf_data.get('CONFIG_FTP')
	subdir('ftp')
endif
if conf_data.get('CONFIG_GEMINI')
	subdir('gemini')
endif
if conf_data.get('CONFIG_GOPHER')
	subdir('gopher')
endif
if conf_data.get('CONFIG_NNTP')
	subdir('nntp')
endif
if conf_data.get('CONFIG_SMB')
	subdir('smb')
endif
if conf_data.get('CONFIG_URI_REWRITE')
	subdir('rewrite')
endif
subdir('auth')
subdir('file')
subdir('http')

if conf_data.get('CONFIG_DATA')
	srcs += files('data.c')
endif
srcs += files('about.c', 'common.c', 'date.c', 'header.c', 'protocol.c', 'proxy.c', 'uri.c', 'user.c')
