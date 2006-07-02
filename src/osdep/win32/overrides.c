
/* Elinks overrides for Win32 (MSVC + MingW)
 * Loosely based on osdep/beos/overrides.c by Mikulas Patocka.
 *
 * By G. Vanem <giva@bgnett.no>  2004 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WIN32_OVERRIDES_SELF

#include "osdep/system.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h"
#include "osdep/win32/overrides.h"
#include "osdep/win32/vt100.h"
#include "osdep/win32/win32.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"

#define SOCK_SHIFT  1024


enum fd_types {
	FDT_FILE = 1,
	FDT_SOCKET,
	FDT_TERMINAL,
	FDT_PIPE
};


static BOOL console_nbio = FALSE;

static const char *keymap[] = {
	"\E[5~", /* VK_PRIOR */
	"\E[6~", /* VK_NEXT */
	"\E[F",  /* VK_END */
	"\E[H",  /* VK_HOME */
	"\E[D",  /* VK_LEFT */
	"\E[A",  /* VK_UP */
	"\E[C",  /* VK_RIGHT */
	"\E[B",  /* VK_DOWN */
	"",      /* VK_SELECT */
	"",      /* VK_PRINT */
	"",      /* VK_EXECUTE */
	"",      /* VK_SNAPSHOT */
	"\E[2~", /* VK_INSERT */
	"\E[3~"  /* VK_DELETE */
};

static const char *keymap_2[] = {
	"\E[11~", /* VK_F1 */
	"\E[12~", /* VK_F2 */
	"\E[13~",  /* VK_F3 */
	"\E[14~",  /* VK_F4 */
	"\E[15~",  /* VK_F5 */
	"\E[17~",  /* VK_F6 */
	"\E[18~",  /* VK_F7 */
	"\E[19~",  /* VK_F8 */
	"\E[20~",  /* VK_F9 */
	"\E[21~",  /* VK_F10 */
	"\E[23~",  /* VK_F11 */
	"\E[24~"   /* VK_F12 */
};

#define TRACE(m...)					\
	do {						\
		if (get_cmd_opt_int("verbose") == 2)	\
			DBG(m);				\
	} while (0)

static enum fd_types
what_fd_type(int *fd)
{
	if (*fd == (int)GetStdHandle(STD_INPUT_HANDLE)
	    || *fd == (int)GetStdHandle(STD_OUTPUT_HANDLE)
	    || *fd == (int)GetStdHandle(STD_ERROR_HANDLE))
		return FDT_TERMINAL;

	if (GetFileType((HANDLE) *fd) == FILE_TYPE_PIPE)
		return FDT_PIPE;

	if (*fd >= SOCK_SHIFT) {
		*fd -= SOCK_SHIFT;
		return FDT_SOCKET;
	}

	return FDT_FILE;
}

static int
console_key_read(const KEY_EVENT_RECORD *ker, char *buf, int max)
{
	char c;
	int  vkey;

	if (!ker->bKeyDown)
		return 0;

	c = ker->uChar.AsciiChar;
	if (c) {
		*buf = c;
		return 1;
	}

	vkey = ker->wVirtualKeyCode;
	if (vkey >= VK_PRIOR && vkey <= VK_DELETE) {
		strncpy(buf, keymap[vkey - VK_PRIOR], max);
	} else {
		if (vkey >= VK_F1 && vkey <= VK_F12) {
			strncpy(buf, keymap_2[vkey - VK_F1], max);
		} else {
			return 0;
		}
	}
	return strlen(buf);
}

#ifdef CONFIG_MOUSE
static int
console_mouse_read(const MOUSE_EVENT_RECORD *mer, char *buf, int max)
{
	struct term_event ev;
	int len;

	if (mer->dwEventFlags || !mer->dwButtonState)  /* not mouse down, or shifts */
		return 0;

	set_mouse_term_event(&ev,
			     1 + mer->dwMousePosition.X,
			     1 + mer->dwMousePosition.Y,
			     B_DOWN);

	len = int_min(max, sizeof(ev));
	memcpy(buf, &ev, len);

	return len;
}
#endif

static int
console_read(HANDLE hnd, void *buf, int max, INPUT_RECORD *irp)
{
	INPUT_RECORD ir;
	const KEY_EVENT_RECORD   *ker = &ir.Event.KeyEvent;
	const MOUSE_EVENT_RECORD *mer = &ir.Event.MouseEvent;
	DWORD num_events;

	ReadConsoleInput (hnd, &ir, 1, &num_events);

	if (irp)
		*irp = ir;

	switch (ir.EventType) {
	case KEY_EVENT:
		TRACE ("KEY_EVENT: ch '%c', %s", ker->uChar.AsciiChar, ker->bKeyDown ? "down" : "up");
		return console_key_read (ker, buf, max);

#ifdef CONFIG_MOUSE
	case MOUSE_EVENT:
		TRACE("MOUSE_EVENT: flg %d, button-state %d, x %lu, y %lu",
		      mer->dwEventFlags, mer->dwButtonState,
		      mer->dwMousePosition.X, mer->dwMousePosition.Y);
		return console_mouse_read(mer, buf, max);
#endif

	case WINDOW_BUFFER_SIZE_EVENT:
		strncpy(buf, "\E[R", max);
		return int_min(max, 3);
	}

	return 0;
}

static int
console_peek(HANDLE hnd)
{
	DWORD num = 0;
	char  buf[10];
	int   rc = 0;

	if (!GetNumberOfConsoleInputEvents(hnd, &num) || num == 0)
		return 0;

	while (num--) {
		INPUT_RECORD ir;
		DWORD written;

		if (console_read(hnd, buf, sizeof(buf), &ir) > 0) {
			/* Put the record back */
			WriteConsoleInput(hnd, &ir, 1, &written);
			rc++;
		}
	}

	return rc;
}

int
win32_write(int fd, const void *buf, unsigned len)
{
	DWORD written = 0;
	int   rc = -1;
	int   orig_fd = fd;

	switch (what_fd_type(&fd)) {
	case FDT_FILE:
		rc = write(fd, buf, len);
		break;

	case FDT_PIPE:
		WriteFile((HANDLE) fd, buf, len, &written, NULL);
		rc = written;
		if (rc != len)
			errno = GetLastError();
		break;

	case FDT_TERMINAL:
		if (isatty(STDOUT_FILENO) > 0) {
#if 0
			WriteConsole ((HANDLE) fd, buf, len, &written, NULL);
			rc = written;
#endif
			rc = VT100_decode((HANDLE) fd, buf, len);
		} else {
			/* stdout redirected */
			rc = write(STDOUT_FILENO, buf, len);
		}
		break;

	case FDT_SOCKET:
		rc = send(fd, buf, len, 0);
		if (rc != len)
			errno = WSAGetLastError();
		break;
	}

	TRACE("fd %d, buf 0x%p, len %u -> rc %d; %s",
	      orig_fd, buf, len, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_read(int fd, void *buf, unsigned len)
{
	int   rc = -1;
	int   orig_fd = fd;
	DWORD Read;

	switch (what_fd_type(&fd)) {
	case FDT_FILE:
		rc = read(fd, buf, len);
		break;

	case FDT_PIPE:
		if (!ReadFile((HANDLE) fd, buf, len, &Read, NULL))
			rc = -1;
		else
			rc = Read;
		break;

	case FDT_TERMINAL:
		rc = console_read((HANDLE) fd, buf, len, NULL);
		if (rc == 0)
			TRACE("read 0 !!");
		break;

	case FDT_SOCKET:
		rc = recv(fd, buf, len, 0);
		if (rc < 0)
			errno = WSAGetLastError();
		break;
	}

	TRACE("fd %d, buf 0x%p, len %u -> rc %d; %s",
	      orig_fd, buf, len, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_close(int fd)
{
	int rc = -1;
	int orig_fd = fd;

	switch (what_fd_type(&fd)) {
	case FDT_FILE:
		rc = close(fd);
		break;

	case FDT_PIPE:
		if (CloseHandle((HANDLE)fd))
			rc = 0;
		break;

	case FDT_TERMINAL:
		rc = 0;   /* nop */
		break;

	case FDT_SOCKET:
		rc = closesocket(fd);
		if (rc < 0)
			errno = WSAGetLastError();
		break;
	}

	TRACE("fd %d -> rc %d; %s",
	      orig_fd, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_ioctl(int fd, long option, int *flag)
{
	char  cmd[20];
	int  rc = 0, orig_fd = fd, flg = *flag;
	DWORD mode;

	switch (what_fd_type(&fd)) {
	case FDT_TERMINAL:
		if (option == FIONBIO)
			console_nbio = flg;
		break;

	case FDT_PIPE:
		assert(option == FIONBIO);

		mode = flg ? PIPE_NOWAIT : PIPE_WAIT;
		mode |= PIPE_READMODE_BYTE;
		SetNamedPipeHandleState((HANDLE) fd, &mode, NULL, NULL);
		rc = 0;
		break;

	case FDT_SOCKET:
		rc = ioctlsocket(fd, option, (unsigned long *) flag);
		if (rc < 0)
			errno = WSAGetLastError();
		break;

	default:
		rc = 0;
		break;
	}

	if (option == FIONBIO)
		strcpy (cmd, "FIONBIO");
	else
		snprintf (cmd, sizeof(cmd), "0x%08lX", option);

	TRACE("fd %d, cmd %s, flag %d -> flag %d, rc %d; %s",
	      orig_fd, cmd, flg, *flag, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_socket(int pf, int type, int protocol)
{
	SOCKET s = socket(pf, type, protocol);
	int    rc;

	if (s == INVALID_SOCKET) {
		rc = -1;
		errno = WSAGetLastError();
	} else {
		rc = s + SOCK_SHIFT;
	}

	TRACE("family %d, type %d, proto %d -> rc %d", pf, type, protocol, rc);

	return rc;
}

int
win32_connect(int fd, struct sockaddr *addr, int addr_len)
{
	int rc = connect(fd - SOCK_SHIFT, addr, addr_len);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d -> rc %d; %s", fd, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_getpeername (int fd, struct sockaddr *addr, int *addr_len)
{
	int rc = getpeername(fd - SOCK_SHIFT, addr, addr_len);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d -> rc %d; %s", fd, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_getsockname(int fd, struct sockaddr *addr, int *addr_len)
{
	int rc = getsockname(fd - SOCK_SHIFT, addr, addr_len);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d -> rc %d; %s", fd, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_accept(int fd, struct sockaddr *addr, int *addr_len)
{
	int rc = accept(fd - SOCK_SHIFT, addr, addr_len);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d -> rc %d; %s", fd, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_listen(int fd, int backlog)
{
	int rc = listen(fd - SOCK_SHIFT, backlog);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d, backlog %d -> rc %d %s",
	      fd, backlog, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_bind(int fd, struct sockaddr *addr, int addr_len)
{
	int rc = bind(fd-SOCK_SHIFT, addr, addr_len);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d -> rc %d; %s", fd, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_getsockopt(int fd, int level, int option, void *optval, int *optlen)
{
	int rc = getsockopt(fd - SOCK_SHIFT, level, option, optval, optlen);

	if (rc < 0)
		errno = WSAGetLastError();

	TRACE("fd %d, level %d, option %d -> rc %d; %s",
	      fd, level, option, rc, rc < 0 ? win32_strerror(errno) : "okay");

	return rc;
}

int
win32_pipe(int *fds)
{
	HANDLE rd_pipe, wr_pipe;

	if (!CreatePipe(&rd_pipe,&wr_pipe,NULL,0)) {
		errno = WSAGetLastError();
		return (-1);
	}

	fds[0] = (int) rd_pipe;
	fds[1] = (int) wr_pipe;

	return 0;
}

static const char *
timeval_str(const struct timeval *tv)
{
	static unsigned char buf[30];

	snprintf(buf, sizeof(buf), "%ld.%03ld", tv->tv_sec, tv->tv_usec/1000);

	return buf;
}

static char *
fd_set_str(char *buf, size_t len, const fd_set *fd, int num_fds)
{
	char *p   = buf;
	char *end = buf + len;
	int   i, num;

	for (i = num = 0; i < num_fds; i++) {
		if (FD_ISSET(i,fd)) {
			p += sprintf (p, "%d,", i);
			num++;
		}

		if (p > end - 12) {
			strcat(p, "<overflow>");
			return buf;
		}
	}

	if (num == 0)
		return "<none set>";

	return buf;
}

static void
select_dump(int num_fds, const fd_set *rd, const fd_set *wr, const fd_set *ex)
{
	char buf_rd[512], buf_wr[512], buf_ex[512];

	TRACE("\tread-fds:   %s\n"
	      "\twrite-fds:  %s\n"
	      "\texcept-fds: %s\n",
	      rd ? fd_set_str(buf_rd,sizeof(buf_rd),rd,num_fds) : "<none>",
	      wr ? fd_set_str(buf_wr,sizeof(buf_wr),wr,num_fds) : "<none>",
	      ex ? fd_set_str(buf_ex,sizeof(buf_ex),ex,num_fds) : "<none>");
}

static int select_read (int fd, struct fd_set *rd)
{
	int rc = 0;
	HANDLE hnd = (HANDLE) fd;

	if (hnd == GetStdHandle(STD_INPUT_HANDLE)) {
		if (console_peek(hnd)) {
			FD_SET(fd, rd);
			rc++;
		}

	} else {
		hnd = (HANDLE) _get_osfhandle(fd);
		if (WaitForSingleObject(hnd, 0) == WAIT_OBJECT_0) {
			FD_SET (fd, rd);
			rc++;
		}
	}

	return rc;
}

static int
select_one_loop(int num_fds, struct fd_set *rd, struct fd_set *wr,
		struct fd_set *ex)
{
	struct timeval tv = { 0, 100 };
	int    rc, fd;

	for (rc = fd = 0; fd < num_fds; fd++) {
		HANDLE hnd = (HANDLE)fd;

		if (GetFileType(hnd) == FILE_TYPE_PIPE) {
			DWORD read = 0;
			if (PeekNamedPipe(hnd, NULL, 0, NULL, &read, NULL)
			    && read > 0) {
				FD_SET (fd, rd);
				rc++;
			}

		} else if (fd < SOCK_SHIFT) {
			rc += select_read(fd, rd);
			if (wr && FD_ISSET(fd,wr))
				rc++;   /* assume always writable */

		} else {
			/* A Winsock socket */
			fd_set sock_rd, sock_wr, sock_ex;
			int    sel_rc;

			FD_ZERO(&sock_rd);
			FD_ZERO(&sock_wr);
			FD_ZERO(&sock_ex);

			FD_SET(fd - SOCK_SHIFT, &sock_rd);
			FD_SET(fd - SOCK_SHIFT, &sock_wr);
			FD_SET(fd - SOCK_SHIFT, &sock_ex);

			sel_rc = select(fd + 1, &sock_rd, NULL, NULL, &tv);
			if (sel_rc > 0) {
				FD_SET(fd, rd);
				rc++;
			} else if (sel_rc < 0) {
				errno = WSAGetLastError();
			}

			if (wr) {
				sel_rc = select(fd + 1, NULL, &sock_wr, NULL, &tv);
				if (sel_rc > 0) {
					FD_SET (fd, wr);
					rc++;
				} else if (sel_rc < 0) {
					errno = WSAGetLastError();
				}
			}

			sel_rc = select(fd + 1, NULL, NULL, &sock_ex, &tv);
			if (sel_rc > 0) {
				FD_SET (fd, ex);
				rc++;
			} else if (sel_rc < 0) {
				errno = WSAGetLastError();
			}
		}
	}

	return (rc);
}

int win32_select (int num_fds, struct fd_set *rd, struct fd_set *wr,
		struct fd_set *ex, struct timeval *tv)
{
	struct fd_set tmp_rd, tmp_ex;
	struct timeval expiry, start_time;
	int    fd, rc;
	BOOL   expired = FALSE;

	TRACE("in: num-fd %d, %s%s%s, tv %s",
	      num_fds, rd ? "r" : "-", wr ? "w" : "-",
	      ex ? "x" : "-", tv ? timeval_str(tv) : "NULL");

	if (get_cmd_opt_int("verbose") == 2)
		select_dump(num_fds, rd, wr, ex);

	FD_ZERO(&tmp_rd);
	FD_ZERO(&tmp_ex);

	if (tv) {
		gettimeofday(&start_time, NULL);

		expiry.tv_sec  = start_time.tv_sec  + tv->tv_sec;
		expiry.tv_usec = start_time.tv_usec + tv->tv_usec;

		/* TODO: Move to gettimeofday() implementation? --jonas */
		while (expiry.tv_usec >= 1000000L) {
			expiry.tv_usec -= 1000000L;
			expiry.tv_sec++;
		}
	}

	errno = 0;

	for (rc = 0; !expired; ) {
		rc += select_one_loop (num_fds, &tmp_rd, wr, &tmp_ex);

		if (tv) {
			struct timeval now;

			gettimeofday(&now, NULL);
			if (now.tv_sec > expiry.tv_sec
			    || (now.tv_sec == expiry.tv_sec
				&& now.tv_usec >= expiry.tv_usec))
				expired = TRUE;
		}

		if (rc) break;
	}

	rc = 0;
	for (fd = 0; fd < num_fds; fd++) {
		if (rd && FD_ISSET(fd, rd) && !FD_ISSET(fd, &tmp_rd))
			FD_CLR(fd, rd);
		else rc++;
		if (wr && FD_ISSET(fd, wr)) rc++;
		/* wr always set */
		if (ex && FD_ISSET(fd, ex) && !FD_ISSET(fd, &tmp_ex))
			FD_CLR(fd, ex);
		else rc++;
	}

	TRACE("-> rc %d, err %d", rc, rc < 0 ? errno : 0);

	if (get_cmd_opt_int("verbose") == 2)
		select_dump(num_fds, rd, wr, ex);

	if (get_cmd_opt_int("verbose") == 2)
		Sleep (400);
	else
		Sleep (0);

	return rc;
}

/* This function handles most Winsock errors we're able to produce. */
static char *
get_winsock_error(int err, char *buf, size_t len)
{
	char *p;

	switch (err) {
	case WSAEINTR:
		p = "Call interrupted.";
		break;
	case WSAEBADF:
		p = "Bad file";
		break;
	case WSAEACCES:
		p = "Bad access";
		break;
	case WSAEFAULT:
		p = "Bad argument";
		break;
	case WSAEINVAL:
		p = "Invalid arguments";
		break;
	case WSAEMFILE:
		p = "Out of file descriptors";
		break;
	case WSAEWOULDBLOCK:
		p = "Call would block";
		break;
	case WSAEINPROGRESS:
	case WSAEALREADY:
		p = "Blocking call progress";
		break;
	case WSAENOTSOCK:
		p = "Descriptor is not a socket.";
		break;
	case WSAEDESTADDRREQ:
		p = "Need destination address";
		break;
	case WSAEMSGSIZE:
		p = "Bad message size";
		break;
	case WSAEPROTOTYPE:
		p = "Bad protocol";
		break;
	case WSAENOPROTOOPT:
		p = "Protocol option is unsupported";
		break;
	case WSAEPROTONOSUPPORT:
		p = "Protocol is unsupported";
		break;
	case WSAESOCKTNOSUPPORT:
		p = "Socket is unsupported";
		break;
	case WSAEOPNOTSUPP:
		p = "Operation not supported";
		break;
	case WSAEAFNOSUPPORT:
		p = "Address family not supported";
		break;
	case WSAEPFNOSUPPORT:
		p = "Protocol family not supported";
		break;
	case WSAEADDRINUSE:
		p = "Address already in use";
		break;
	case WSAEADDRNOTAVAIL:
		p = "Address not available";
		break;
	case WSAENETDOWN:
		p = "Network down";
		break;
	case WSAENETUNREACH:
		p = "Network unreachable";
		break;
	case WSAENETRESET:
		p = "Network has been reset";
		break;
	case WSAECONNABORTED:
		p = "Connection was aborted";
		break;
	case WSAECONNRESET:
		p = "Connection was reset";
		break;
	case WSAENOBUFS:
		p = "No buffer space";
		break;
	case WSAEISCONN:
		p = "Socket is already connected";
		break;
	case WSAENOTCONN:
		p = "Socket is not connected";
		break;
	case WSAESHUTDOWN:
		p = "Socket has been shut down";
		break;
	case WSAETOOMANYREFS:
		p = "Too many references";
		break;
	case WSAETIMEDOUT:
		p = "Timed out";
		break;
	case WSAECONNREFUSED:
		p = "Connection refused";
		break;
	case WSAELOOP:
		p = "Loop??";
		break;
	case WSAENAMETOOLONG:
		p = "Name too long";
		break;
	case WSAEHOSTDOWN:
		p = "Host down";
		break;
	case WSAEHOSTUNREACH:
		p = "Host unreachable";
		break;
	case WSAENOTEMPTY:
		p = "Not empty";
		break;
	case WSAEPROCLIM:
		p = "Process limit reached";
		break;
	case WSAEUSERS:
		p = "Too many users";
		break;
	case WSAEDQUOT:
		p = "Bad quota";
		break;
	case WSAESTALE:
		p = "Something is stale";
		break;
	case WSAEREMOTE:
		p = "Remote error";
		break;
	case WSAEDISCON:
		p = "Disconnected";
		break;

		/* Extended Winsock errors */
	case WSASYSNOTREADY:
		p = "Winsock library is not ready";
		break;
	case WSANOTINITIALISED:
		p = "Winsock library not initalised";
		break;
	case WSAVERNOTSUPPORTED:
		p = "Winsock version not supported.";
		break;

		/* getXbyY() errors (already handled in herrmsg):
		   Authoritative Answer: Host not found */
	case WSAHOST_NOT_FOUND:
		p = "Host not found";
		break;

		/* Non-Authoritative: Host not found, or SERVERFAIL */
	case WSATRY_AGAIN:
		p = "Host not found, try again";
		break;

		/* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
	case WSANO_RECOVERY:
		p = "Unrecoverable error in call to nameserver";
		break;

		/* Valid name, no data record of requested type */
	case WSANO_DATA:
		p = "No data record of requested type";
		break;

	default:
		return NULL;
	}

	strncpy(buf, p, len);
	buf[len - 1] = '\0';

	return buf;
}

/* A smarter strerror() */
char *
win32_strerror(int err)
{
	static char buf[512];
	char  *p;

	if (err >= 0 && err < sys_nerr) {
		strncpy(buf, strerror(err), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';

	} else {
		if (!get_winsock_error(err, buf, sizeof(buf))
		    && !FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
				      LANG_NEUTRAL, buf, sizeof(buf)-1, NULL))
			sprintf (buf, "Unknown error %d (%#x)", err, err);
	}

	/* strip trailing '\r\n' or '\n'. */
	if ((p = strrchr(buf,'\n')) != NULL && (p - buf) >= 2)
		*p = '\0';
	if ((p = strrchr(buf,'\r')) != NULL && (p - buf) >= 1)
		*p = '\0';

	return buf;
}
