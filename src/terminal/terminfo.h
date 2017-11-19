#ifndef EL__TERMINAL_TERMINFO_H
#define EL__TERMINAL_TERMINFO_H

int terminfo_setupterm(char *term, int fildes);
char *terminfo_set_bold(int arg);
char *terminfo_set_italics(int arg);
char *terminfo_set_underline(int arg);
char *terminfo_set_foreground(int arg);
char *terminfo_set_background(int arg);

#endif
