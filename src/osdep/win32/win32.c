/* Win32 support fo ELinks. It has pretty different life than rest of ELinks. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Get SHGFP_TYPE_CURRENT from <shlobj.h>.  */
#define _WIN32_IE 0x500

#include <windows.h>
#include <shlobj.h>

#include "osdep/system.h"

#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include "elinks.h"

#include "main/select.h"
#include "osdep/win32/win32.h"
#include "osdep/osdep.h"
#include "terminal/terminal.h"


void
init_osdep(void)
{
#ifdef CONFIG_OS_WIN32
	WSADATA ws;
	WORD ver = MAKEWORD(1,1);
#ifdef CONFIG_IPV6
	ver = MAKEWORD(2,0);;
#endif
	if (WSAStartup(ver,&ws) != 0) {
		printf("Failed to initialise Winsock ver %d.%d\n", ver >> 8, ver & 255);
		exit(-1);
	}
#endif
#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
#ifdef CONFIG_IDN
	{
		char buf[60];
		UINT cp = GetACP();

		if (!getenv("CHARSET") && cp > 0)
		{
			snprintf (buf, sizeof(buf), "CHARSET=cp%u", cp);
			putenv (buf);
		}
	}
#endif
}

void
terminate_osdep(void)
{
}


int
get_system_env(void)
{
	return (0);
}

void
handle_terminal_resize(int fd, void (*fn)())
{
}

void
unhandle_terminal_resize(int fd)
{
}

void
get_terminal_size(int fd, int *x, int *y)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

	if (!*x) {
		*x = get_e("COLUMNS");
		if (!*x)
			*x = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	}
	if (!*y) {
		*y = get_e("LINES");
		if (!*y)
			*y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	}
}


int
exe(unsigned char *path)
{
	int rc;
	unsigned char *shell = get_shell();
	unsigned char *x = *path != '"' ? " /c start /wait " : " /c start /wait \"\" ";
	unsigned char *p = malloc((strlen(shell) + strlen(x) + strlen(path)) * 2 + 1);

	if (!p)
		return -1;

	strcpy(p, shell);
	strcat(p, x);
	strcat(p, path);
	x = p;
	while (*x) {
		if (*x == '\\') {
			memmove(x + 1, x, strlen(x) + 1);
			x++;
		}
		x++;
	}
	rc = system(p);

	free(p);

	return rc;
}


int
get_ctl_handle(void)
{
	return get_input_handle();
}


#if defined(HAVE_BEGINTHREAD)

struct tdata {
	void (*fn)(void *, int);
	int h;
	unsigned char data[1];
};

extern void bgt(struct tdata *t);

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	struct tdata *t;

	if (c_pipe(p) < 0)
		return -1;

	t = malloc(sizeof(*t) + l);
	if (!t)
		return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (_beginthread((void (*)(void *)) bgt, 65536, t) == -1) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}
#endif


int
get_input_handle(void)
{
	static HANDLE hStdIn = INVALID_HANDLE_VALUE;

	if (hStdIn == INVALID_HANDLE_VALUE) {
		DWORD dwMode;

		SetConsoleTitle("ELinks - Console mode browser");

		hStdIn = GetStdHandle(STD_INPUT_HANDLE);
		GetConsoleMode(hStdIn, &dwMode);
		dwMode &= ~(ENABLE_LINE_INPUT |	ENABLE_ECHO_INPUT);
		dwMode |= (ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
		SetConsoleMode(hStdIn, dwMode);
	}
	return (int) hStdIn;
}

int
get_output_handle(void)
{
	static HANDLE hStdOut = INVALID_HANDLE_VALUE;

	if (hStdOut == INVALID_HANDLE_VALUE)
		hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	return (int) hStdOut;
}

int
gettimeofday(struct timeval* p, void* tz)
{
	union {
		long long ns100; /*time since 1 Jan 1601 in 100ns units */
		FILETIME ft;
	} _now;

	assertm (p != NULL);
	GetSystemTimeAsFileTime (&_now.ft);
	p->tv_usec = (long) ((_now.ns100 / 10LL) % 1000000LL);
	p->tv_sec  = (long) ((_now.ns100 - 116444736000000000LL) / 10000000LL);
	return 0;
}


int
mkstemp(char *template)
{
	char tempname[MAX_PATH];
	char pathname[MAX_PATH];

 	/* Get the directory for temp files */
	GetTempPath(MAX_PATH, pathname);

	/* Create a temporary file. */
	GetTempFileName(pathname, template, 0, tempname);

	return open(tempname, O_CREAT | O_WRONLY | O_EXCL | O_BINARY);
}

int
tcgetattr(int fd, struct termios *_termios_p)
{
	(void) fd;
	(void) _termios_p;
	return 0;
}

int
tcsetattr(int fd, int _optional_actions, const struct termios *_termios_p)
{
	(void) fd;
	(void) _optional_actions;
	(void) _termios_p;
	return 0;
}

#ifdef ENABLE_NLS
int
gettext__parse(void *arg)
{
	return 0;
}
#endif

unsigned char *
user_appdata_directory(void)
{
#if _WIN32_WINNT >= 0x0500
	HWND hwnd = GetConsoleWindow();
#else
	HWND hwnd = NULL;
#endif
	char path[MAX_PATH];
	HRESULT hr;

	hr = SHGetFolderPath(hwnd, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path);
	if (hr == S_OK) /* Don't even allow S_FALSE.  */
		return stracpy(path);
	else
		return NULL;
}
