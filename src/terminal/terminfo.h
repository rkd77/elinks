#ifndef EL__TERMINAL_TERMINFO_H
#define EL__TERMINAL_TERMINFO_H

#ifdef __cplusplus
extern "C" {
#endif

int terminfo_setupterm(char *term, int fildes);
const char *terminfo_clear_screen(void);
const char *terminfo_set_bold(int arg);
const char *terminfo_set_italics(int arg);
const char *terminfo_set_underline(int arg);
const char *terminfo_set_foreground(int arg);
const char *terminfo_set_background(int arg);
const char *terminfo_set_standout(int arg);
const char *terminfo_set_strike(int arg);
const char *terminfo_orig_pair(void);

int terminfo_max_colors(void);
const char *terminfo_cursor_address(int y, int x);

#ifdef __cplusplus
}
#endif

#endif
