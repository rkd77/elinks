srcs += files('cookies.c', 'dialogs.c', 'path.c', 'parser.c')

exe = executable('cookies-t', 'cookies-t.c', 'path.c', include_directories:['..'], build_by_default:false)
test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})
t = find_program('test-cookies-t')
test('cookies', t, depends:[exe], env:test_lib, workdir:meson.current_build_dir())

parsetstdeps = files('parser.c', 'parsetst.c')

executable('parsetst', parsetstdeps, testdeps, dependencies:[iconvdeps], c_args:['-DHAVE_CONFIG_H'], include_directories:['.', '..', '../..'])
