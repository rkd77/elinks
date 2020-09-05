if conf_data.get('CONFIG_BROTLI')
	srcs += files('brotli.c')
endif
if conf_data.get('CONFIG_BZIP2')
	srcs += files('bzip2.c')
endif
if conf_data.get('CONFIG_GZIP')
	srcs += files('deflate.c')
endif
if conf_data.get('CONFIG_LZMA')
	srcs += files('lzma.c')
endif
if conf_data.get('CONFIG_ZSTD')
	srcs += files('zstd.c')
endif

srcs += files('encoding.c')
