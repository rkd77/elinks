#ifndef EL__OSDEP_OSDEP_H
#define EL__OSDEP_OSDEP_H

#include "osdep/beos/beos.h"
#include "osdep/os2/os2.h"
#include "osdep/riscos/riscos.h"
#include "osdep/unix/unix.h"
#include "osdep/win32/win32.h"

#ifndef CHAR_DIR_SEP
#define CHAR_DIR_SEP '/'
#endif

#ifndef STRING_DIR_SEP
#define STRING_DIR_SEP "/"
#endif

int get_system_env(void);
int get_e(unsigned char *env);
int is_xterm(void);
int is_twterm(void);
void get_terminal_size(int, int *, int *);
void handle_terminal_resize(int, void (*)(void));
void unhandle_terminal_resize(int);
void set_bin(int);
int c_pipe(int *);
int get_input_handle(void);
int get_output_handle(void);
int get_ctl_handle(void);
void want_draw(void);
void done_draw(void);
void init_osdep(void);
void terminate_osdep(void);
void *handle_mouse(int, void (*)(void *, unsigned char *, int), void *);
void unhandle_mouse(void *);
void suspend_mouse(void *);
void resume_mouse(void *);
int start_thread(void (*)(void *, int), void *, int);
unsigned char *get_clipboard_text(void);
void set_clipboard_text(unsigned char *);
void set_window_title(unsigned char *, int codepage);
unsigned char *get_window_title(int codepage);
void block_stdin(void);
void unblock_stdin(void);
int exe(unsigned char *);
int resize_window(int, int, int, int);
int can_resize_window(int);
int can_open_os_shell(int);
void set_highpri(void);

#ifdef USE_OPEN_PREALLOC
int open_prealloc(char *, int, int, off_t);
void prealloc_truncate(int, off_t);
#else
static inline void prealloc_truncate(int x, int y) { }
#endif

unsigned char *get_system_str(int);

int set_nonblocking_fd(int);
int set_blocking_fd(int);
void set_ip_tos_throughput(int socket);

unsigned char *get_cwd(void);
void set_cwd(unsigned char *);

unsigned char *get_shell(void);

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

/* We define own cfmakeraw() wrapper because cfmakeraw() is broken on AIX,
 * thus we fix it right away. We can also emulate cfmakeraw() if it is not
 * available at all. Face it, we are just cool. */
void elinks_cfmakeraw(struct termios *t);

#ifndef user_appdata_directory
#define user_appdata_directory() NULL
#endif

#endif
