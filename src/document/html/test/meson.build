t = executable('parse-meta-refresh-test', 'parse-meta-refresh-test.c', testdeps, meson.current_source_dir() / '../parse-meta-refresh.c', dependencies:[iconvdeps],
c_args:['-DHAVE_CONFIG_H'], include_directories:['.', '..', '../..', '../../..', '../../../..'])
test('parse-meta-refresh', t)
