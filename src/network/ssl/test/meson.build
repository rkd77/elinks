t = executable('match-hostname-test', 'match-hostname-test.c',  meson.source_root()+'/src/network/ssl/match-hostname.c', testdeps, dependencies:[iconvdeps],
c_args:['-DHAVE_CONFIG_H'], include_directories:['.', '..', '../..', '../../..', '../../../..'])
test('match-hostname-test', t)
