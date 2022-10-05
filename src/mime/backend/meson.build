if conf_data.get('CONFIG_DGI')
	srcs += files('dgi.c')
endif
if conf_data.get('CONFIG_MAILCAP')
	srcs += files('mailcap.c')
endif
if conf_data.get('CONFIG_MIMETYPES')
	srcs += files('mimetypes.c')
endif

srcs += files('common.c', 'default.c')

executable('mailcap-cache', 'common.c', 'mailcap.c', meson.source_root()+'/src/osdep/osdep.c', testdeps, c_args:['-DHAVE_CONFIG_H', '-DTEST_MAILCAP'], include_directories:['.','..','../..','../../..'])
test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})
t = find_program('test-mailcap-cache')
test('mailcap-cache', t, env: test_lib, workdir: meson.current_build_dir())
