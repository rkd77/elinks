executable('html-mangle', 'html-mangle.c',
meson.source_root()+'/src/dom/configuration.c',
meson.source_root()+'/src/dom/node.c',
meson.source_root()+'/src/dom/select.c',
meson.source_root()+'/src/dom/stack.c',
meson.source_root()+'/src/dom/scanner.c',
meson.source_root()+'/src/dom/css/scanner.c',
testdeps,
c_args:['-DHAVE_CONFIG_H'], cpp_args:['-DHAVE_CONFIG_H'], include_directories:['.', '../..', '../../..'])

executable('sgml-parser', 'sgml-parser.c',
meson.source_root()+'/src/dom/configuration.c',
meson.source_root()+'/src/dom/node.c',
meson.source_root()+'/src/dom/select.c',
meson.source_root()+'/src/dom/stack.c',
meson.source_root()+'/src/dom/scanner.c',
meson.source_root()+'/src/dom/css/scanner.c',
meson.source_root()+'/src/dom/sgml/dump.c',
meson.source_root()+'/src/dom/sgml/parser.c',
meson.source_root()+'/src/dom/sgml/scanner.c',
meson.source_root()+'/src/dom/sgml/sgml.c',

meson.source_root()+'/src/dom/sgml/docbook/docbook.c',
meson.source_root()+'/src/dom/sgml/html/html.c',
meson.source_root()+'/src/dom/sgml/rss/rss.c',
meson.source_root()+'/src/dom/sgml/xbel/xbel.c',
testdeps,
c_args:['-DHAVE_CONFIG_H'], cpp_args:['-DHAVE_CONFIG_H'], include_directories:['.', '../..', '../../..'])

test_lib = environment({'TEST_LIB': meson.source_root()+'/test/libtest.sh'})

t1 = find_program('test-dom-configuration-basic')
t2 = find_program('test-sgml-dump-basic')
t3 = find_program('test-sgml-parser-basic')
t4 = find_program('test-sgml-parser-error')
t5 = find_program('test-sgml-parser-incomplete')
t6 = find_program('test-sgml-parser-incremental')
t7 = find_program('test-sgml-parser-lines')
t8 = find_program('test-sgml-parser-random')

test('dom-configuration-basic', t1, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
test('sgml-dump-basic', t2, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
test('sgml-parser-basic', t3, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
test('sgml-parser-error', t4, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
test('sgml-parser-incomplete', t5, env: test_lib, workdir: meson.current_build_dir())
test('sgml-parser-incremental', t6, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
test('sgml-parser-lines', t7, env: test_lib, workdir: meson.current_build_dir())
test('sgml-parser-random', t8, env: test_lib, workdir: meson.current_build_dir(), should_fail: true)
