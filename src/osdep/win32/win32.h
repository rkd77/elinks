
#ifndef EL__OSDEP_WIN32_WIN32_H
#define EL__OSDEP_WIN32_WIN32_H

#ifdef CONFIG_OS_WIN32

#undef CHAR_DIR_SEP
#define CHAR_DIR_SEP '\\'
#undef STRING_DIR_SEP
#define STRING_DIR_SEP "\\"

struct terminal;

void open_in_new_win32(struct terminal *term, unsigned char *exe_name,
		       unsigned char *param);
unsigned char *user_appdata_directory(void);
#define user_appdata_directory user_appdata_directory


/* Stub functions: */

int mkstemp (char *template);
int gettimeofday (struct timeval *tv, void *tz);

/* fake termios for Win32 (excluding CygWin) */

#if !defined(__CYGWIN__)

#define VMIN     6
#define VTIME    11
#define TCSANOW  3
#define O_NOCTTY 0

#define ECHO     0x00000100
#define ICANON   0x00001000
#define IEXTEN   0x00002000
#define ECHONL   0x00000800

#define CSIZE    0x00000C00
#define OPOST    0x00000100
#define BRKINT   0x00000100
#define ICRNL    0x00000200
#define IGNBRK   0x00000400
#define IGNCR    0x00000800
#define INLCR    0x00002000
#define ISIG     0x00004000
#define ISTRIP   0x00008000

#define IXOFF    0x00010000
#define IXON     0x00020000
#define PARMRK   0x00040000
#define PARENB   0x00004000
#define CS8      0x00000C00

#define NCCS     12

struct termios {
       unsigned  c_cflag;
       unsigned  c_lflag;
       unsigned  c_iflag;
       unsigned  c_oflag;
       unsigned  c_cc [NCCS];
};

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
#endif /* __CYGWIN__ */

#endif /* CONFIG_OS_WIN32 */
#endif
