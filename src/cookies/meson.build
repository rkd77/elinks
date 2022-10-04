srcs += files('cookies.c', 'dialogs.c', 'path.c', 'parser.c')

executable('cookies-t', 'cookies-t.c', 'path.c', include_directories:['..'])
test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})
t = find_program('test-cookies-t')
test('cookies', t, env: test_lib, workdir: meson.current_source_dir())
