idna_dep = dependency('libidn')
executable('uri-test', 'uri-test.c', 'stub.c', meson.current_source_dir()+'/../protocol.cpp', meson.current_source_dir()+'/../uri.c',
meson.source_root()+'/src/intl/gettext/bindtextdom.c',
meson.source_root()+'/src/intl/gettext/dcgettext.c',
meson.source_root()+'/src/intl/gettext/dcigettext.c',
meson.source_root()+'/src/intl/gettext/dcngettext.c',
meson.source_root()+'/src/intl/gettext/dgettext.c',
meson.source_root()+'/src/intl/gettext/dngettext.c',
meson.source_root()+'/src/intl/gettext/explodename.c',
meson.source_root()+'/src/intl/gettext/finddomain.c',
meson.source_root()+'/src/intl/gettext/gettext.c',
meson.source_root()+'/src/intl/gettext/intl-compat.c',
meson.source_root()+'/src/intl/gettext/l10nflist.c',
meson.source_root()+'/src/intl/gettext/libintl.c',
meson.source_root()+'/src/intl/gettext/loadmsgcat.c',
meson.source_root()+'/src/intl/gettext/localcharset.c',
meson.source_root()+'/src/intl/gettext/localealias.c',
meson.source_root()+'/src/intl/gettext/ngettext.c',
meson.source_root()+'/src/intl/gettext/plural.c',
meson.source_root()+'/src/intl/gettext/textdomain.c',
testdeps, dependencies: [idna_dep],
c_args:['-DHAVE_CONFIG_H'], cpp_args:['-DHAVE_CONFIG_H'],  include_directories:['.', '..', '../..', '../../..', '../../../..'])
t = find_program('test-normalize-uri')
test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})
test('normalize-uri', t, env: test_lib, workdir: meson.current_build_dir())

t2 = find_program('test-get-translated-uri')
test('get-translated-uri', t2, env: test_lib, workdir: meson.current_build_dir(), should_fail: false)
