t = executable('match-hostname-test', 'match-hostname-test.c',  meson.current_source_dir() + '/../match-hostname.c', testdeps, c_args:['-DHAVE_CONFIG_H'], include_directories:['.', '..', '../..', '../../..', '../../../..'])
test('match-hostname-test', t)
