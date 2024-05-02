/* Win32 support fo ELinks. It has pretty different life than rest of ELinks. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Get SHGFP_TYPE_CURRENT from <shlobj.h>.  */
#define _WIN32_IE 0x500

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include <shlobj.h>

#include "osdep/system.h"

#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>

#include "elinks.h"

#include "main/select.h"
#include "main/timer.h"
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
	setlocale(LC_ALL, "");
#ifdef CONFIG_IDN2
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

/* Terminal size */
static void (*terminal_resize_callback)(void);
static timer_id_T terminal_resize_timer = TIMER_ID_UNDEF;
static int old_xsize, old_ysize;

static void
terminal_resize_fn(void *unused)
{
	int cur_xsize, cur_ysize;
	int cw, ch;

	install_timer(&terminal_resize_timer, TERMINAL_POLL_TIMEOUT, terminal_resize_fn, NULL);
	get_terminal_size(0, &cur_xsize, &cur_ysize, &cw, &ch);

	if ((old_xsize != cur_xsize) || (old_ysize != cur_ysize)) {
		old_xsize = cur_xsize;
		old_ysize = cur_ysize;
		(*terminal_resize_callback)();
	}
}

static void
terminal_resize_poll(int x, int y)
{
	if (terminal_resize_timer != TIMER_ID_UNDEF) {
		elinks_internal("terminal_resize_poll: timer already active");
	}
	old_xsize = x;
	old_ysize = y;
	install_timer(&terminal_resize_timer, TERMINAL_POLL_TIMEOUT, terminal_resize_fn, NULL);
}

void
handle_terminal_resize(int fd, void (*fn)(void))
{
	int x, y;
	int cw, ch;

	terminal_resize_callback = fn;
	get_terminal_size(fd, &x, &y, &cw, &ch);
	terminal_resize_poll(x, y);
}

void
unhandle_terminal_resize(int fd)
{
	if (terminal_resize_timer != TIMER_ID_UNDEF) {
		kill_timer(&terminal_resize_timer);
	}
}

void
get_terminal_size(int fd, int *x, int *y, int *cw, int *ch)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);

	if (x) {
		*x = csbi.srWindow.Right - csbi.srWindow.Left + 1;

		if (!*x) {
			*x = get_e("COLUMNS");
		}
	}
	if (y) {
		*y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

		if (!*y) {
			*y = get_e("LINES");
		}
	}

	if (cw) {
		*cw = 8;
	}

	if (ch) {
		*ch = 16;
	}
}


int
exe(char *path)
{
	int rc;
	char *shell = get_shell();
	char *x = *path != '"' ? " /c start /wait " : " /c start /wait \"\" ";
	char *p = malloc((strlen(shell) + strlen(x) + strlen(path)) * 2 + 1);

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
		SetConsoleTitle("ELinks - Console mode browser");

		hStdIn = GetStdHandle(STD_INPUT_HANDLE);
		SetConsoleMode(hStdIn, ENABLE_EXTENDED_FLAGS);
		SetConsoleMode(hStdIn, (ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT));
	}
	return (int) hStdIn;
}

int
get_output_handle(void)
{
	static HANDLE hStdOut = INVALID_HANDLE_VALUE;

	if (hStdOut == INVALID_HANDLE_VALUE) {
		DWORD dwMode;

		hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		GetConsoleMode(hStdOut, &dwMode);
		SetConsoleMode(hStdOut, dwMode);
	}
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
mkstemp(char *template_)
{
	char pathname[MAX_PATH];

 	/* Get the directory for temp files */
	GetTempPath(MAX_PATH, pathname);

	/* Create a temporary file. */
	GetTempFileName(pathname, "ABC", 0, template_);

	return open(template_, O_WRONLY | O_BINARY | O_EXCL);
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

char *
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

long
os_get_free_mem_in_mib(void)
{
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof(statex);
	GlobalMemoryStatusEx(&statex);

	return statex.ullAvailPhys / (1024 * 1024);
}
