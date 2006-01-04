/* BeOS system-specific routines emulating POSIX. */

/* Note that this file is currently unmaintained and basically dead. Noone
 * cares about BeOS support, apparently. This file may yet survive for some
 * time, but it will probably be removed if noone will adopt it. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BEOS_SELF

#include "osdep/system.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <be/kernel/OS.h>

#include "elinks.h"

#include "osdep/beos/overrides.h"


#define SHS	128

int
be_read(int s, void *ptr, int len)
{
	if (s >= SHS) return recv(s - SHS, ptr, len, 0);
	return read(s, ptr, len);
}

int
be_write(int s, void *ptr, int len)
{
	if (s >= SHS) return send(s - SHS, ptr, len, 0);
	return write(s, ptr, len);
}

int
be_close(int s)
{
	if (s >= SHS) return closesocket(s - SHS);
	return close(s);
}

int
be_socket(int pf, int sock, int prot)
{
	int h = socket(pf, sock, prot);

	if (h < 0) return h;
	return h + SHS;
}

int
be_connect(int s, struct sockaddr *sa, int sal)
{
	return connect(s - SHS, sa, sal);
}

int
be_getpeername(int s, struct sockaddr *sa, int *sal)
{
	return getpeername(s - SHS, sa, sal);
}

int
be_getsockname(int s, struct sockaddr *sa, int *sal)
{
	return getsockname(s - SHS, sa, sal);
}

int
be_listen(int s, int c)
{
	return listen(s - SHS, c);
}

int
be_accept(int s, struct sockaddr *sa, int *sal)
{
	int a = accept(s - SHS, sa, sal);

	if (a < 0) return -1;
	return a + SHS;
}

int
be_bind(int s, struct sockaddr *sa, int sal)
{
#if 0
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;

	if (!sin->sin_port) {
		int i;
		for (i = 16384; i < 49152; i++) {
			sin->sin_port = i;
			if (!be_bind(s, sa, sal)) return 0;
		}
		return -1;
	}
#endif
	if (bind(s - SHS, sa, sal)) return -1;
	getsockname(s - SHS, sa, &sal);
	return 0;
}

#define PIPE_RETRIES	10

int
be_pipe(int *fd)
{
	int s1, s2, s3, l;
	struct sockaddr_in sa1, sa2;
	int retry_count = 0;

again:
	s1 = be_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s1 < 0) {
		/*perror("socket1");*/
		goto fatal_retry;
	}

	s2 = be_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s2 < 0) {
		/*perror("socket2");*/
		be_close(s1);
		goto fatal_retry;
	}
	memset(&sa1, 0, sizeof(sa1));
	sa1.sin_family = AF_INET;
	sa1.sin_port = 0;
	sa1.sin_addr.s_addr = INADDR_ANY;
	if (be_bind(s1, (struct sockaddr *) &sa1, sizeof(sa1))) {
		/*perror("bind");*/

clo:
		be_close(s1);
		be_close(s2);
		goto fatal_retry;
	}
	if (be_listen(s1, 1)) {
		/*perror("listen");*/
		goto clo;
	}
	if (be_connect(s2, (struct sockaddr *) &sa1, sizeof(sa1))) {
		/*perror("connect");*/
		goto clo;
	}
	l = sizeof(sa2);
	if ((s3 = be_accept(s1, (struct sockaddr *) &sa2, &l)) < 0) {
		/*perror("accept");*/
		goto clo;
	}
	be_getsockname(s3, (struct sockaddr *) &sa1, &l);
	if (sa1.sin_addr.s_addr != sa2.sin_addr.s_addr) {
		be_close(s3);
		goto clo;
	}
	be_close(s1);
	fd[0] = s2;
	fd[1] = s3;
	return 0;

fatal_retry:
	if (++retry_count > PIPE_RETRIES) return -1;
	sleep(1);
	goto again;
}

int
be_select(int n, struct fd_set *rd, struct fd_set *wr, struct fd_set *exc,
	  struct timeval *tm)
{
	int i, s;
	struct fd_set d, rrd;

	FD_ZERO(&d);
	if (!rd) rd = &d;
	if (!wr) wr = &d;
	if (!exc) exc = &d;
	if (n >= FDSETSIZE) n = FDSETSIZE;
	FD_ZERO(exc);
	for (i = 0; i < n; i++) if ((i < SHS && FD_ISSET(i, rd)) || FD_ISSET(i, wr)) {
		for (i = SHS; i < n; i++) FD_CLR(i, rd);
		return INT_MAX;
	}
	FD_ZERO(&rrd);
	for (i = SHS; i < n; i++) if (FD_ISSET(i, rd)) FD_SET(i - SHS, &rrd);
	if ((s = select(FD_SETSIZE, &rrd, &d, &d, tm)) < 0) {
		FD_ZERO(rd);
		return 0;
	}
	FD_ZERO(rd);
	for (i = SHS; i < n; i++) if (FD_ISSET(i - SHS, &rrd)) FD_SET(i, rd);
	return s;
}

#ifndef SO_ERROR
#define SO_ERROR	10001
#endif

int
be_getsockopt(int s, int level, int optname, void *optval, int *optlen)
{
	if (optname == SO_ERROR && *optlen >= sizeof(int)) {
		*(int *) optval = 0;
		*optlen = sizeof(int);
		return 0;
	}
	return -1;
}
