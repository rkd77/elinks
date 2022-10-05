srcs += files('ftp.c', 'parse.c')

executable('ftp-parser', 'ftp-parser.c', meson.source_root()+'/src/protocol/date.c', 'parse.c', testdeps,
c_args:['-DHAVE_CONFIG_H'], include_directories:['.','..','../..', '../../..'])

test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})
t = find_program('test-ftp-parser')
test('ftp-parser', t, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
