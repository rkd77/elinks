configure_file(input : 'Doxyfile.in',
               output : 'Doxyfile',
               configuration : conf_data)

dblatex = find_program('dblatex', required: false)
doxygen = find_program('doxygen', required: false)
pod2html = find_program('pod2html', required: false)
xmlto = find_program('xmlto', required: false)

perl_html = []
perl_hooks_html = []

if conf_data.get('CONFIG_REPRODUCIBLE')
    asciidoc_env = ['LC_ALL=C', 'LANGUAGE=en', 'CONFIG_REPRODUCIBLE=1', 'SOURCE_DATE_EPOCH='+conf_data.get('SOURCE_DATE_EPOCH')]
else
    asciidoc_env = []
endif
doc_dir = get_option('prefix') / 'share/doc/elinks'

if pod2html.found()
    perl_html = custom_target('perl.html',
              input: ['perl.pod'],
              output: ['perl.html'],
              install: true,
              install_dir: doc_dir,
              command: [pod2html, '--outfile=@OUTPUT@', '--infile=@INPUT@'])

    perl_hooks_html = custom_target('perl-hooks.html',
              input: ['../contrib/perl/hooks.pl'],
              output: ['perl-hooks.html'],
              install: true,
              install_dir: doc_dir,
              command: [pod2html, '--outfile=@OUTPUT@', '--infile=@INPUT@'])
endif

features_txt = custom_target('features.txt',
               input: ['../features.conf'],
               output: 'features.txt',
               capture: true,
               install: true,
               install_dir: doc_dir,
               command: [meson.current_source_dir() + '/tools/conf2doc', '@INPUT@'])

keymap_actions_txt = custom_target('keymap-actions.txt',
               input: ['../src/config/kbdbind.c'],
               output: 'keymap-actions.txt',
               capture: true,
               command: [meson.current_source_dir() + '/tools/keys2doc', '@INPUT@', 'keymap-actions.txt'])

keymap_defaults_txt = custom_target('keymap-defaults.txt',
               input: ['../src/config/kbdbind.c'],
               output: 'keymap-defaults.txt',
               capture: true,
               install: true,
               install_dir: doc_dir,
               command: [meson.current_source_dir() + '/tools/keys2doc', '@INPUT@', 'keymap-defaults.txt'])

option_command_frag_xml = custom_target('option-command.frag.xml',
               input: [],
               output: 'option-command.frag.xml',
               env: ['LC_ALL=C', 'LANGUAGE=en'],
               command: [meson.current_source_dir() + '/tools/help2xml', elinks, '@OUTPUT@'])

option_config_frag_xml = custom_target('option-config.frag.xml',
               input: [],
               output: 'option-config.frag.xml',
               env: ['LC_ALL=C', 'LANGUAGE=en'],
               command: [meson.current_source_dir() + '/tools/help2xml', elinks, '@OUTPUT@'])

option_command_frag_xhtml = custom_target('option-command.frag.xhtml',
               input: [],
               output: 'option-command.frag.xhtml',
               env: ['LC_ALL=C', 'LANGUAGE=en'],
               install: true,
               install_dir: doc_dir,
               command: [meson.current_source_dir() + '/tools/help2xml', elinks, '@OUTPUT@'])

option_config_frag_xhtml = custom_target('option-config.frag.xhtml',
               input: [],
               output: 'option-config.frag.xhtml',
               env: ['LC_ALL=C', 'LANGUAGE=en'],
               command: [meson.current_source_dir() + '/tools/help2xml', elinks, '@OUTPUT@'])

elinks_1_html = custom_target('elinks.1.html',
                input: 'elinks.1.txt',
                output: 'elinks.1.html',
                depends: option_command_frag_xhtml,
                env: asciidoc_env,
                install: true,
                install_dir: doc_dir,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/xhtml11.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_conf_5_html = custom_target('elinks.conf.5.html',
                input: 'elinks.conf.5.txt',
                output: 'elinks.conf.5.html',
                depends: option_config_frag_xhtml,
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/xhtml11.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinkskeys_5_html = custom_target('elinkskeys.5.html',
                input: 'elinkskeys.5.txt',
                output: 'elinkskeys.5.html',
                depends: [keymap_actions_txt, keymap_defaults_txt],
                env: asciidoc_env,
                install: true,
                install_dir: doc_dir,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/xhtml11.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

hacking_html = custom_target('hacking.html',
                input: 'hacking.txt',
                output: 'hacking.html',
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/xhtml11.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

manual_html = custom_target('manual.html',
                input: 'manual.txt',
                output: 'manual.html',
                depends: features_txt,
                env: asciidoc_env,
                install: true,
                install_dir: doc_dir,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/xhtml11.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_1_xml = custom_target('elinks.1.xml',
                input: 'elinks.1.txt',
                output: 'elinks.1.xml',
                depends: option_command_frag_xml,
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/docbook.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '-d', 'manpage',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_conf_5_xml = custom_target('elinks.conf.5.xml',
                input: 'elinks.conf.5.txt',
                output: 'elinks.conf.5.xml',
                depends: option_config_frag_xml,
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/docbook.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '-d', 'manpage',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinkskeys_5_xml = custom_target('elinkskeys.5.xml',
                input: 'elinkskeys.5.txt',
                output: 'elinkskeys.5.xml',
                depends: [keymap_actions_txt, keymap_defaults_txt],
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/docbook.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '-d', 'manpage',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

hacking_xml = custom_target('hacking.xml',
                input: 'hacking.txt',
                output: 'hacking.xml',
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/docbook.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

manual_xml = custom_target('manual.xml',
                input: 'manual.txt',
                output: 'manual.xml',
                depends: features_txt,
                env: asciidoc_env,
                command: [meson.current_source_dir() + '/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', meson.current_source_dir() + '/tools/asciidoc/asciidoc.conf',
                '-f', meson.current_source_dir() + '/tools/asciidoc/docbook.conf',
                '-f', meson.current_source_dir() + '/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_1 = []
elinks_conf_5 = []
elinkskeys_5 = []
manual_html_chunked = []

man1_dir = join_paths(get_option('prefix'), get_option('mandir'), 'man1')
man5_dir = join_paths(get_option('prefix'), get_option('mandir'), 'man5')

if xmlto.found()
    elinks_1 = custom_target('elinks.1',
                input: [],
                output: 'elinks.1',
                depends: elinks_1_xml,
                install: true,
                install_dir: man1_dir,
                command: [xmlto, '-o', meson.current_build_dir(), 'man', meson.current_build_dir()+'/elinks.1.xml'])

    elinks_conf_5 = custom_target('elinks.conf.5',
                input: [],
                output: 'elinks.conf.5',
                depends: elinks_conf_5_xml,
                install: true,
                install_dir: man5_dir,
                command: [xmlto, '-o', meson.current_build_dir(), 'man', meson.current_build_dir()+'/elinks.conf.5.xml'])

    elinkskeys_5 = custom_target('elinkskeys.5',
                input: [],
                output: 'elinkskeys.5',
                depends: elinkskeys_5_xml,
                install: true,
                install_dir: man5_dir,
                command: [xmlto, '-o', meson.current_build_dir(), 'man', meson.current_build_dir()+'/elinkskeys.5.xml'])

    manual_html_chunked = custom_target('manual.html-chunked',
                input: [],
                output: 'manual.html-chunked',
                depends: manual_xml,
                install: true,
                install_dir: doc_dir,
                command: [xmlto, '-o', meson.current_build_dir() + '/manual.html-chunked', 'html', meson.current_build_dir()+'/manual.xml'])
endif

manual_pdf = []

if dblatex.found()
    manual_pdf = custom_target('manual.pdf',
                input: [],
                output: 'manual.pdf',
                depends: manual_xml,
                install: true,
                install_dir: doc_dir,
                command: [dblatex, meson.current_build_dir() + '/manual.xml'])
endif

doxygen_api = []
if doxygen.found()
    doxygen_api = custom_target('api',
                input: [],
                output: 'api',
                env: ['OUTPUT_DIRECTORY='+meson.current_build_dir()+'/'],
                install: true,
                install_dir: doc_dir,
                command: [doxygen, meson.current_build_dir()+'/Doxyfile'])
endif

txt = alias_target('txt', elinks, features_txt, keymap_actions_txt, keymap_defaults_txt)
alias_target('xml', elinks, option_command_frag_xml, option_config_frag_xml, option_command_frag_xhtml, option_config_frag_xhtml,
    elinks_1_xml, elinks_conf_5_xml, elinkskeys_5_xml, hacking_xml, manual_xml)
alias_target('html', elinks, perl_html, perl_hooks_html, elinks_1_html, elinks_conf_5_html, elinkskeys_5_html, hacking_html, manual_html,
    manual_html_chunked)
alias_target('pdf', elinks, manual_xml, manual_pdf)
alias_target('man', elinks, elinks_1, elinks_conf_5, elinkskeys_5)
