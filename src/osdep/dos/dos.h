#ifndef EL__OSDEP_DOS_DOS_H
#define EL__OSDEP_DOS_DOS_H

#ifdef CONFIG_OS_DOS

#ifdef __cplusplus
extern "C" {
#endif

#define DOS_EXTRA_KEYBOARD

#ifdef DOS_EXTRA_KEYBOARD
#define OS_SETRAW
#endif

#include <sys/types.h>

struct timeval;

int dos_read(int fd, void *buf, size_t size);
int dos_write(int fd, const void *buf, size_t size);
int dos_pipe(int fd[2]);
int dos_close(int fd);
int dos_select(int n, fd_set *rs, fd_set *ws, fd_set *es, struct timeval *t, int from_main_loop);
void save_terminal(void);
void restore_terminal(void);
int dos_setraw(int ctl, int save);
void os_seed_random(unsigned char **pool, int *pool_size);
int os_default_charset(void);


#ifndef DOS_OVERRIDES_SELF

#define read dos_read
#define write dos_write
#define pipe dos_pipe
#define close dos_close

#endif

#ifdef __cplusplus
}
#endif

#endif

#endif
