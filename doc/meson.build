configure_file(input : 'Doxyfile.in',
               output : 'Doxyfile',
               configuration : conf_data)

pod2html = find_program('pod2html', required: false)

if pod2html.found()
    custom_target('perl.html',
              input: ['perl.pod'],
              output: ['perl.html'],
              command: [pod2html, '--outfile=@OUTPUT@', '--infile=@INPUT@'])

    custom_target('perl-hooks.html',
              input: ['../contrib/perl/hooks.pl'],
              output: ['perl-hooks.html'],
              command: [pod2html, '--outfile=@OUTPUT@', '--infile=@INPUT@'])
endif
