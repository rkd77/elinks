configure_file(input : 'Doxyfile.in',
               output : 'Doxyfile',
               configuration : conf_data)

pod2html = find_program('pod2html', required: false)
dblatex = find_program('dblatex', required: false)

perl_html = []
perl_hooks_html = []

if pod2html.found()
    perl_html = custom_target('perl.html',
              input: ['perl.pod'],
              output: ['perl.html'],
              command: [pod2html, '--outfile=@OUTPUT@', '--infile=@INPUT@'])

    perl_hooks_html = custom_target('perl-hooks.html',
              input: ['../contrib/perl/hooks.pl'],
              output: ['perl-hooks.html'],
              command: [pod2html, '--outfile=@OUTPUT@', '--infile=@INPUT@'])
endif

features_txt = custom_target('features.txt',
               input: ['../features.conf'],
               output: 'features.txt',
               capture: true,
               command: ['@CURRENT_SOURCE_DIR@/tools/conf2doc', '@INPUT@'])

keymap_actions_txt = custom_target('keymap-actions.txt',
               input: ['../src/config/kbdbind.c'],
               output: 'keymap-actions.txt',
               capture: true,
               command: ['@CURRENT_SOURCE_DIR@/tools/keys2doc', '@INPUT@', 'keymap-actions.txt'])

keymap_defaults_txt = custom_target('keymap-defaults.txt',
               input: ['../src/config/kbdbind.c'],
               output: 'keymap-defaults.txt',
               capture: true,
               command: ['@CURRENT_SOURCE_DIR@/tools/keys2doc', '@INPUT@', 'keymap-defaults.txt'])

option_command_frag_xml = custom_target('option-command.frag.xml',
               input: [],
               output: 'option-command.frag.xml',
               command: ['LC_ALL=C', 'LANGUAGE=en', '@CURRENT_SOURCE_DIR@/tools/help2xml', elinks, '@OUTPUT@'])

option_config_frag_xml = custom_target('option-config.frag.xml',
               input: [],
               output: 'option-config.frag.xml',
               command: ['LC_ALL=C', 'LANGUAGE=en', '@CURRENT_SOURCE_DIR@/tools/help2xml', elinks, '@OUTPUT@'])

option_command_frag_xhtml = custom_target('option-command.frag.xhtml',
               input: [],
               output: 'option-command.frag.xhtml',
               command: ['LC_ALL=C', 'LANGUAGE=en', '@CURRENT_SOURCE_DIR@/tools/help2xml', elinks, '@OUTPUT@'])

option_config_frag_xhtml = custom_target('option-config.frag.xhtml',
               input: [],
               output: 'option-config.frag.xhtml',
               command: ['LC_ALL=C', 'LANGUAGE=en', '@CURRENT_SOURCE_DIR@/tools/help2xml', elinks, '@OUTPUT@'])

elinks_1_html = custom_target('elinks.1.html',
                input: 'elinks.1.txt',
                output: 'elinks.1.html',
                depends: option_command_frag_xhtml,
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/xhtml11.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_conf_5_html = custom_target('elinks.conf.5.html',
                input: 'elinks.conf.5.txt',
                output: 'elinks.conf.5.html',
                depends: option_config_frag_xhtml,
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/xhtml11.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinkskeys_5_html = custom_target('elinkskeys.5.html',
                input: 'elinkskeys.5.txt',
                output: 'elinkskeys.5.html',
                depends: [keymap_actions_txt, keymap_defaults_txt],
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/xhtml11.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

hacking_html = custom_target('hacking.html',
                input: 'hacking.txt',
                output: 'hacking.html',
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/xhtml11.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

manual_html = custom_target('manual.html',
                input: 'manual.txt',
                output: 'manual.html',
                depends: features_txt,
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/xhtml11.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'xhtml11',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_1_xml = custom_target('elinks.1.xml',
                input: 'elinks.1.txt',
                output: 'elinks.1.xml',
                depends: option_command_frag_xml,
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/docbook.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinks_conf_5_xml = custom_target('elinks.conf.5.xml',
                input: 'elinks.conf.5.txt',
                output: 'elinks.conf.5.xml',
                depends: option_config_frag_xml,
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/docbook.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

elinkskeys_5_xml = custom_target('elinkskeys.5.xml',
                input: 'elinkskeys.5.txt',
                output: 'elinkskeys.5.xml',
                depends: [keymap_actions_txt, keymap_defaults_txt],
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/docbook.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

hacking_xml = custom_target('hacking.xml',
                input: 'hacking.txt',
                output: 'hacking.xml',
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/docbook.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

manual_xml = custom_target('manual.xml',
                input: 'manual.txt',
                output: 'manual.xml',
                depends: features_txt,
                command: ['@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.py',
                '--no-conf', '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/asciidoc.conf',
                '-f', '@CURRENT_SOURCE_DIR@/tools/asciidoc/docbook.conf',
                '-f', '@CURRENT_SOURCE_DIR@/asciidoc.conf',
                '-a', 'builddir='+meson.current_build_dir()+'/',
                '-a', 'elinks_version='+meson.project_version(),
                '-b', 'docbook',
                '--unsafe', '-o', '@OUTPUT@', '@INPUT@'])

manual_pdf = []

if dblatex.found()
    manual_pdf = custom_target('manual.pdf',
                input: [],
                output: 'manual.pdf',
                depends: manual_xml,
                command: [dblatex, meson.current_build_dir() + '/manual.xml'])
endif

txt = alias_target('txt', features_txt, keymap_actions_txt, keymap_defaults_txt)
alias_target('xml', option_command_frag_xml, option_config_frag_xml, option_command_frag_xhtml, option_config_frag_xhtml,
    elinks_1_xml, elinks_conf_5_xml, elinkskeys_5_xml, hacking_xml, manual_xml)
alias_target('html', perl_html, perl_hooks_html, elinks_1_html, elinks_conf_5_html, elinkskeys_5_html, hacking_html, manual_html)
alias_target('pdf', manual_xml, manual_pdf)
