idna_dep = dependency('libidn')

uri_test_files = files('uri-test.c', 'stub.c', meson.current_source_dir()+'/../protocol.cpp', meson.current_source_dir()+'/../uri.c')

exe = executable('uri-test', uri_test_files,
testdeps, dependencies: [iconvdeps, idna_dep, mozjsdeps, wattdeps],
c_args:['-DHAVE_CONFIG_H'], cpp_args:['-DHAVE_CONFIG_H'],  include_directories:['.', '..', '../..', '../../..', '../../../..'])
t = find_program('test-normalize-uri')
test_lib = environment({'TEST_LIB':meson.source_root()+'/test/libtest.sh'})
test('normalize-uri', t, depends:[exe], env:test_lib, workdir:meson.current_build_dir())

t2 = find_program('test-get-translated-uri')
test('get-translated-uri', t2, depends:[exe], env:test_lib, workdir:meson.current_build_dir(), should_fail:false)
