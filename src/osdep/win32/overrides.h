
#ifndef EL__OSDEP_WIN32_OVERRIDES_H
#define EL__OSDEP_WIN32_OVERRIDES_H

#ifdef CONFIG_OS_WIN32

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif


int win32_write(int fd, const void *buf, unsigned len);
int win32_read(int fd, void *buf, unsigned len);
int win32_pipe(int *fds);
int win32_socket(int pf, int type, int proto);
int win32_connect(int fd, struct sockaddr *addr, int addr_len);
int win32_getpeername(int fd, struct sockaddr *addr, int *addr_len);
int win32_getsockname(int fd, struct sockaddr *addr, int *addr_len);
int win32_listen(int fd, int backlog);
int win32_accept(int fd, struct sockaddr *addr, int *addr_len);
int win32_bind(int fd, struct sockaddr *addr, int addr_len);
int win32_close(int fd);
int win32_getsockopt(int fd, int level, int option, void *optval, int *optlen);
int win32_ioctl(int fd, long option, int *flag);
int win32_select(int num_fds, struct fd_set *rd, struct fd_set *wr,
		 struct fd_set *ex, struct timeval *tv);
char *win32_strerror(int err);


#ifndef WIN32_OVERRIDES_SELF

#define read				win32_read
#define write				win32_write
#define close				win32_close
#define pipe				win32_pipe
#define socket(pf, type, prot)		win32_socket(pf, type, prot)
#define connect(fd, a, al)		win32_connect(fd, a, al)
#define getpeername(fd, a, al)		win32_getpeername(fd, a, al)
#define getsockname(fd, a, al)		win32_getsockname(fd, a, al)
#define listen(fd, bl)			win32_listen(fd, bl)
#define accept(fd, a, al)		win32_accept(fd, a, al)
#define bind(fd, a, al)	 		win32_bind(fd, a, al)
#define getsockopt(fd, l, o, ov, ol)	win32_getsockopt(fd, l, o, ov, ol)
#define ioctl(fd,opt,flag)		win32_ioctl(fd, opt, flag)
#define select				win32_select
#define strerror(err)			win32_strerror(err)

#endif

#endif
#endif
