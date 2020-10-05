#ifndef EL__TERMINAL_TERMINFO_H
#define EL__TERMINAL_TERMINFO_H

#ifdef __cplusplus
extern "C" {
#endif

int terminfo_setupterm(char *term, int fildes);
char *terminfo_clear_screen(void);
char *terminfo_set_bold(int arg);
char *terminfo_set_italics(int arg);
char *terminfo_set_underline(int arg);
char *terminfo_set_foreground(int arg);
char *terminfo_set_background(int arg);
int terminfo_max_colors(void);
char *terminfo_cursor_address(int y, int x);

#ifdef __cplusplus
}
#endif

#endif
