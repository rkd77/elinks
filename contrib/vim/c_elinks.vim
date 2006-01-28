" Setting Vim to support the ELinks coding style
"
" To use this file, drop it in ~/.vim/ftplugin and set filetype plugin on.
" Finally, make sure the path to the source directory contains the word
" 'elinks', for example ~/src/elinks/.
"
" For .h files, link it as cpp_elinks.vim or define c_syntax_for_h in ~/.vimrc.
" For .inc files, let g:filetype_inc = 'c' in ~/.vimrc.

if expand('%:p:h') =~ '.*elinks.*'
  setlocal shiftwidth=8
  setlocal tabstop=8
  setlocal softtabstop=0
  setlocal noexpandtab
endif
