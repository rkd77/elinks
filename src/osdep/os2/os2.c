/* OS/2 support fo ELinks. It has pretty different life than rest of ELinks. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef X2
/* from xf86sup - XFree86 OS/2 support driver */
#include <pty.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "elinks.h"

#include "osdep/os2/os2.h"
#include "osdep/osdep.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "util/conv.h"


#define INCL_MOU
#define INCL_VIO
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WINCLIPBOARD
#define INCL_WINSWITCHLIST
#include <os2.h>
#include <io.h>
#include <process.h>
#include <sys/video.h>
#ifdef HAVE_SYS_FMUTEX_H
#include <sys/builtin.h>
#include <sys/fmutex.h>
#endif


#define A_DECL(type, var) type var##1, var##2, *var = _THUNK_PTR_STRUCT_OK(&var##1) ? &var##1 : &var##2

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) xt = !!getenv("WINDOWID");

	return xt;
}

int winch_pipe[2];
int winch_thread_running = 0;

#define WINCH_SLEEPTIME 500 /* time in ms for winch thread to sleep */

static void
winch_thread(void)
{
	/* A thread which regularly checks whether the size of
	   window has changed. Then raise SIGWINCH or notifiy
	   the thread responsible to handle this. */
	static int old_xsize, old_ysize;
	static int cur_xsize, cur_ysize;

	signal(SIGPIPE, SIG_IGN);
	if (get_terminal_size(0, &old_xsize, &old_ysize)) return;
	while (1) {
		if (get_terminal_size(0, &cur_xsize, &cur_ysize)) return;
		if ((old_xsize != cur_xsize) || (old_ysize != cur_ysize)) {
			old_xsize = cur_xsize;
			old_ysize = cur_ysize;
			write(winch_pipe[1], "x", 1);
			/* Resizing may take some time. So don't send a flood
			 * of requests?! */
			_sleep2(2 * WINCH_SLEEPTIME);
		} else {
			_sleep2(WINCH_SLEEPTIME);
		}
	}
}

static void
winch(void *s)
{
	unsigned char c;

	while (can_read(winch_pipe[0]) && safe_read(winch_pipe[0], &c, 1) == 1);
	((void (*)()) s)();
}

void
handle_terminal_resize(int fd, void (*fn)())
{
	if (!is_xterm()) return;
	if (!winch_thread_running) {
		if (c_pipe(winch_pipe) < 0) return;
		winch_thread_running = 1;
		_beginthread((void (*)(void *)) winch_thread, NULL, 0x32000, NULL);
	}
	set_handlers(winch_pipe[0], winch, NULL, NULL, fn);
}

void
unhandle_terminal_resize(int fd)
{
	clear_handlers(winch_pipe[0]);
}

void
get_terminal_size(int fd, int *x, int *y)
{
	if (is_xterm()) {
#ifdef X2
		/* int fd; */
		int arc;
		struct winsize win;

		/* fd = STDIN_FILENO; */
		arc = ptioctl(1, TIOCGWINSZ, &win);
		if (arc) {
/*
			DBG("%d", errno);
*/
			*x = DEFAULT_TERMINAL_WIDTH;
			*y = DEFAULT_TERMINAL_HEIGHT;
			return 0;
		}
		*y = win.ws_row;
		*x = win.ws_col;
/*
		DBG("%d %d", *x, *y);
*/

#else
		*x = DEFAULT_TERMINAL_WIDTH;
		*y = DEFAULT_TERMINAL_HEIGHT;
#endif
	} else {
		int a[2] = {0, 0};

		_scrsize(a);
		*x = a[0];
		*y = a[1];
		if (!*x) {
			*x = get_e("COLUMNS");
			if (!*x) *x = DEFAULT_TERMINAL_WIDTH;
		}
		if (!*y) {
			*y = get_e("LINES");
			if (!*y) *y = DEFAULT_TERMINAL_HEIGHT;
		}
	}
}


int
exe(unsigned char *path)
{
	int flags = P_SESSION;
	int pid;
	int ret = -1;
	unsigned char *shell = get_shell();

	if (is_xterm()) flags |= P_BACKGROUND;

	pid = spawnlp(flags, shell, shell, "/c", path, (char *) NULL);
	if (pid != -1)
		waitpid(pid, &ret, 0);

	return ret;
}

unsigned char *
get_clipboard_text(void)
{
	PTIB tib;
	PPIB pib;
	HAB hab;
	HMQ hmq;
	ULONG oldType;
	unsigned char *ret = 0;

	DosGetInfoBlocks(&tib, &pib);

	oldType = pib->pib_ultype;

	pib->pib_ultype = 3;

	hab = WinInitialize(0);
	if (hab != NULLHANDLE) {
		hmq = WinCreateMsgQueue(hab, 0);
		if (hmq != NULLHANDLE) {
			if (WinOpenClipbrd(hab)) {
				ULONG fmtInfo = 0;

				if (WinQueryClipbrdFmtInfo(hab, CF_TEXT, &fmtInfo) != FALSE) {
					ULONG selClipText = WinQueryClipbrdData(hab, CF_TEXT);

					if (selClipText) {
						PCHAR pchClipText = (PCHAR) selClipText;

						ret = stracpy(pchClipText);
					}
				}
				WinCloseClipbrd(hab);
			}
			WinDestroyMsgQueue(hmq);
		}
		WinTerminate(hab);
	}

	pib->pib_ultype = oldType;

	return ret;
}

void
set_clipboard_text(unsigned char *data)
{
	PTIB tib;
	PPIB pib;
	HAB hab;
	HMQ hmq;
	ULONG oldType;

	DosGetInfoBlocks(&tib, &pib);

	oldType = pib->pib_ultype;

	pib->pib_ultype = 3;

	hab = WinInitialize(0);
	if (hab != NULLHANDLE) {
		hmq = WinCreateMsgQueue(hab, 0);
		if (hmq != NULLHANDLE) {
			if (WinOpenClipbrd(hab)) {
				PVOID pvShrObject = NULL;
				if (DosAllocSharedMem(&pvShrObject, NULL, strlen(data) + 1, PAG_COMMIT | PAG_WRITE | OBJ_GIVEABLE) == NO_ERROR) {
					strcpy(pvShrObject, data);
					WinSetClipbrdData(hab, (ULONG) pvShrObject, CF_TEXT, CFI_POINTER);
				}
				WinCloseClipbrd(hab);
			}
			WinDestroyMsgQueue(hmq);
		}
		WinTerminate(hab);
	}

	pib->pib_ultype = oldType;
}

unsigned char *
get_window_title(int codepage)
{
#ifndef DEBUG_OS2
	unsigned char *org_switch_title;
	unsigned char *org_win_title = NULL;
	static PTIB tib = NULL;
	static PPIB pib = NULL;
	ULONG oldType;
	HSWITCH hSw = NULLHANDLE;
	SWCNTRL swData;
	HAB hab;
	HMQ hmq;

	/* save current process title */

	if (!pib) DosGetInfoBlocks(&tib, &pib);
	oldType = pib->pib_ultype;
	memset(&swData, 0, sizeof(swData));
	if (hSw == NULLHANDLE) hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
	if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
		/*org_switch_title = mem_alloc(strlen(swData.szSwtitle)+1);
		strcpy(org_switch_title, swData.szSwtitle);*/
		/* Go to PM */
		pib->pib_ultype = 3;
		hab = WinInitialize(0);
		if (hab != NULLHANDLE) {
			hmq = WinCreateMsgQueue(hab, 0);
			if (hmq != NULLHANDLE) {
				org_win_title = mem_alloc(MAXNAMEL + 1);
				if (org_win_title)
					WinQueryWindowText(swData.hwnd,
							   MAXNAMEL + 1,
							   org_win_title);
					org_win_title[MAXNAMEL] = 0;
				/* back From PM */
				WinDestroyMsgQueue(hmq);
			}
			WinTerminate(hab);
		}
		pib->pib_ultype = oldType;
	}
	return org_win_title;
#else
	return NULL;
#endif
}

void
set_window_title(unsigned char *title, int codepage)
{
#ifndef DEBUG_OS2
	static PTIB tib;
	static PPIB pib;
	ULONG oldType;
	static HSWITCH hSw;
	SWCNTRL swData;
	HAB hab;
	HMQ hmq;
	unsigned char new_title[MAXNAMEL];

	if (!title) return;
	if (!pib) DosGetInfoBlocks(&tib, &pib);
	oldType = pib->pib_ultype;
	memset(&swData, 0, sizeof(swData));
	if (hSw == NULLHANDLE) hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
	if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
		unsigned char *p;

		safe_strncpy(new_title, title, MAXNAMEL - 1);
		sanitize_title(new_title);
		safe_strncpy(swData.szSwtitle, new_title, MAXNAMEL - 1);
		WinChangeSwitchEntry(hSw, &swData);
		/* Go to PM */
		pib->pib_ultype = 3;
		hab = WinInitialize(0);
		if (hab != NULLHANDLE) {
			hmq = WinCreateMsgQueue(hab, 0);
			if (hmq != NULLHANDLE) {
				if (swData.hwnd)
					WinSetWindowText(swData.hwnd, new_title);
				/* back From PM */
				WinDestroyMsgQueue(hmq);
			}
			WinTerminate(hab);
		}
	}
	pib->pib_ultype = oldType;
#endif
}

#if 0
void
set_window_title(int init, const char *url)
{
	static char *org_switch_title;
	static char *org_win_title;
	static PTIB tib;
	static PPIB pib;
	static ULONG oldType;
	static HSWITCH hSw;
	static SWCNTRL swData;
	HAB hab;
	HMQ hmq;
	char new_title[MAXNAMEL];

	switch (init) {
	case 1:
		DosGetInfoBlocks(&tib, &pib);
		oldType = pib->pib_ultype;
		memset(&swData, 0, sizeof(swData));
		hSw = WinQuerySwitchHandle(0, pib->pib_ulpid);
		if (hSw != NULLHANDLE && !WinQuerySwitchEntry(hSw, &swData)) {
			org_switch_title = mem_alloc(strlen(swData.szSwtitle) + 1);
			strcpy(org_switch_title, swData.szSwtitle);
			pib->pib_ultype = 3;
			hab = WinInitialize(0);
			hmq = WinCreateMsgQueue(hab, 0);
			if (hab != NULLHANDLE && hmq != NULLHANDLE) {
				org_win_title = mem_alloc(MAXNAMEL + 1);
				WinQueryWindowText(swData.hwnd, MAXNAMEL + 1, org_win_title);
				WinDestroyMsgQueue(hmq);
				WinTerminate(hab);
			}
			pib->pib_ultype = oldType;
		}
		break;
	case -1:
		pib->pib_ultype = 3;
		hab = WinInitialize(0);
		hmq = WinCreateMsgQueue(hab, 0);
		if (hSw != NULLHANDLE && hab != NULLHANDLE && hmq != NULLHANDLE) {
			safe_strncpy(swData.szSwtitle, org_switch_title, MAXNAMEL);
			WinChangeSwitchEntry(hSw, &swData);

			if (swData.hwnd)
				WinSetWindowText(swData.hwnd, org_win_title);
			WinDestroyMsgQueue(hmq);
			WinTerminate(hab);
		}
		pib->pib_ultype = oldType;
		mem_free(org_switch_title);
		mem_free(org_win_title);
		break;
	case 0:
		if (url && *url) {
			safe_strncpy(new_title, url, MAXNAMEL - 10);
			strcat(new_title, " - Links");
			pib->pib_ultype = 3;
			hab = WinInitialize(0);
			hmq = WinCreateMsgQueue(hab, 0);
			if (hSw != NULLHANDLE && hab != NULLHANDLE && hmq != NULLHANDLE) {
				safe_strncpy(swData.szSwtitle, new_title, MAXNAMEL);
				WinChangeSwitchEntry(hSw, &swData);

				if (swData.hwnd)
					WinSetWindowText(swData.hwnd, new_title);
				WinDestroyMsgQueue(hmq);
				WinTerminate(hab);
			}
			pib->pib_ultype = oldType;
		}
		break;
	}
}
#endif

int
resize_window(int x, int y, int old_width, int old_height)
{
	A_DECL(VIOMODEINFO, vmi);

	resize_count++;
	if (is_xterm()) return -1;
	vmi->cb = sizeof(*vmi);
	if (VioGetMode(vmi, 0)) return -1;
	vmi->col = x;
	vmi->row = y;
	if (VioSetMode(vmi, 0)) return -1;
#if 0
	unsigned char cmdline[16];
	sprintf(cmdline, "mode ");
	ulongcat(cmdline + 5, NULL, x, 5, 0);
	strcat(cmdline, ",");
	ulongcat(cmdline + strlen(cmdline), NULL, y, 5, 0);
#endif
	return 0;
}


#if OS2_MOUSE

#ifdef HAVE_SYS_FMUTEX_H
_fmutex mouse_mutex;
int mouse_mutex_init = 0;
#endif
int mouse_h = -1;

struct os2_mouse_spec {
	int p[2];
	void (*fn)(void *, unsigned char *, int);
	void *data;
	unsigned char buffer[sizeof(struct interlink_event)];
	int bufptr;
	int terminate;
};

void
mouse_thread(void *p)
{
	int status;
	struct os2_mouse_spec *oms = p;
	A_DECL(HMOU, mh);
	A_DECL(MOUEVENTINFO, ms);
	A_DECL(USHORT, rd);
	A_DECL(USHORT, mask);

	signal(SIGPIPE, SIG_IGN);
	if (MouOpen(NULL, mh)) goto ret;
	mouse_h = *mh;
	*mask = MOUSE_MOTION_WITH_BN1_DOWN | MOUSE_BN1_DOWN |
		MOUSE_MOTION_WITH_BN2_DOWN | MOUSE_BN2_DOWN |
		MOUSE_MOTION_WITH_BN3_DOWN | MOUSE_BN3_DOWN |
		MOUSE_MOTION;
	MouSetEventMask(mask, *mh);
	*rd = MOU_WAIT;
	status = -1;

	while (1) {
		struct interlink_event ev;
		struct interlink_event_mouse mouse;
		int w, ww;

		if (MouReadEventQue(ms, rd, *mh)) break;
#ifdef HAVE_SYS_FMUTEX_H
		_fmutex_request(&mouse_mutex, _FMR_IGNINT);
#endif
		if (!oms->terminate) MouDrawPtr(*mh);
#ifdef HAVE_SYS_FMUTEX_H
		_fmutex_release(&mouse_mutex);
#endif
		mouse.x = ms->col;
		mouse.y = ms->row;
		/*DBG("status: %d %d %d", ms->col, ms->row, ms->fs);*/
		if (ms->fs & (MOUSE_BN1_DOWN | MOUSE_BN2_DOWN | MOUSE_BN3_DOWN))
			mouse.button = status = B_DOWN | (ms->fs & MOUSE_BN1_DOWN ? B_LEFT : ms->fs & MOUSE_BN2_DOWN ? B_MIDDLE : B_RIGHT);
		else if (ms->fs & (MOUSE_MOTION_WITH_BN1_DOWN | MOUSE_MOTION_WITH_BN2_DOWN | MOUSE_MOTION_WITH_BN3_DOWN)) {
			int b = ms->fs & MOUSE_MOTION_WITH_BN1_DOWN ? B_LEFT : ms->fs & MOUSE_MOTION_WITH_BN2_DOWN ? B_MIDDLE : B_RIGHT;

			if (status == -1)
				b |= B_DOWN;
			else
				b |= B_DRAG;
			mouse.button = status = b;
		} else {
			if (status == -1) continue;
			mouse.button = (status & BM_BUTT) | B_UP;
			status = -1;
		}

		set_mouse_interlink_event(&ev, mouse.x, mouse.y, mouse.button);
		if (hard_write(oms->p[1], (unsigned char *) &ev, sizeof(ev)) != sizeof(ev)) break;
	}
#ifdef HAVE_SYS_FMUTEX_H
	_fmutex_request(&mouse_mutex, _FMR_IGNINT);
#endif
	mouse_h = -1;
	MouClose(*mh);
#ifdef HAVE_SYS_FMUTEX_H
	_fmutex_release(&mouse_mutex);
#endif

ret:
	close(oms->p[1]);
	/*free(oms);*/
}

void
mouse_handle(struct os2_mouse_spec *oms)
{
	ssize_t r = safe_read(oms->p[0], oms->buffer + oms->bufptr,
		          sizeof(struct interlink_event) - oms->bufptr);

	if (r <= 0) {
		unhandle_mouse(oms);
		return;
	}

	oms->bufptr += r;
	if (oms->bufptr == sizeof(struct interlink_event)) {
		oms->bufptr = 0;
		oms->fn(oms->data, oms->buffer, sizeof(struct interlink_event));
	}
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	struct os2_mouse_spec *oms;

	if (is_xterm()) return NULL;
#ifdef HAVE_SYS_FMUTEX_H
	if (!mouse_mutex_init) {
		if (_fmutex_create(&mouse_mutex, 0)) return NULL;
		mouse_mutex_init = 1;
	}
#endif
		/* This is never freed but it's allocated only once */
	oms = malloc(sizeof(*oms));
	if (!oms) return NULL;
	oms->fn = fn;
	oms->data = data;
	oms->bufptr = 0;
	oms->terminate = 0;
	if (c_pipe(oms->p)) {
		free(oms);
		return NULL;
	}
	_beginthread(mouse_thread, NULL, 0x10000, (void *) oms);
	set_handlers(oms->p[0], (select_handler_T) mouse_handle, NULL, NULL, oms);

	return oms;
}

void
unhandle_mouse(void *om)
{
	struct os2_mouse_spec *oms = om;

	want_draw();
	oms->terminate = 1;
	clear_handlers(oms->p[0]);
	close(oms->p[0]);
	done_draw();
}

void
suspend_mouse(void *om)
{
}

void
resume_mouse(void *om)
{
}

void
want_draw(void)
{
	A_DECL(NOPTRRECT, pa);
#ifdef HAVE_SYS_FMUTEX_H
	if (mouse_mutex_init) _fmutex_request(&mouse_mutex, _FMR_IGNINT);
#endif
	if (mouse_h != -1) {
		static int x = -1, y = -1;
		static int c = -1;

		if (x == -1 || y == -1 || (c != resize_count)) {
			get_terminal_size(1, &x, &y);
			c = resize_count;
		}

		pa->row = 0;
		pa->col = 0;
		pa->cRow = y - 1;
		pa->cCol = x - 1;
		MouRemovePtr(pa, mouse_h);
	}
}

void
done_draw(void)
{
#ifdef HAVE_SYS_FMUTEX_H
	if (mouse_mutex_init) _fmutex_release(&mouse_mutex);
#endif
}

#endif /* OS2_MOUSE */


int
get_ctl_handle(void)
{
	return get_input_handle();
}


int
get_system_env(void)
{
	int env = get_common_env();

	/* !!! FIXME: telnet */
	if (!is_xterm()) env |= ENV_OS2VIO;

	return env;
}


void
set_highpri(void)
{
	DosSetPriority(PRTYS_PROCESS, PRTYC_FOREGROUNDSERVER, 0, 0);
}


int
can_open_os_shell(int environment)
{
	if (environment & ENV_XWIN) return 0;
	return 1;
}


#if defined(HAVE_BEGINTHREAD)

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	struct tdata *t;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	t = malloc(sizeof(*t) + l);
	if (!t) return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (_beginthread((void (*)(void *)) bgt, NULL, 65536, t) == -1) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}

#if defined(HAVE_READ_KBD)

int tp = -1;
int ti = -1;

void
input_thread(void *p)
{
	unsigned char c[2];
	int h = (int) p;

	signal(SIGPIPE, SIG_IGN);
	while (1) {
#if 0
		c[0] = _read_kbd(0, 1, 1);
		if (c[0]) if (safe_write(h, c, 1) <= 0) break;
		else {
			int w;
			printf("1");fflush(stdout);
			c[1] = _read_kbd(0, 1, 1);
			printf("2");fflush(stdout);
			w = safe_write(h, c, 2);
			printf("3");fflush(stdout);
			if (w <= 0) break;
			if (w == 1) if (safe_write(h, c+1, 1) <= 0) break;
			printf("4");fflush(stdout);
		}
#endif
		/* for the records:
		 * _read_kbd(0, 1, 1) will
		 * read a char, don't echo it, wait for one available and
		 * accept CTRL-C.
		 * Knowing that, I suggest we replace this call completly!
		 */
		*c = _read_kbd(0, 1, 1);
		write(h, c, 1);
	}
	close(h);
}

int
get_input_handle(void)
{
	int fd[2];

	if (ti != -1) return ti;
	if (is_xterm()) return 0;
	if (c_pipe(fd) < 0) return 0;
	ti = fd[0];
	tp = fd[1];
	_beginthread(input_thread, NULL, 0x10000, (void *) tp);
	return fd[0];
}

#endif

#endif


#ifdef USE_OPEN_PREALLOC
int
open_prealloc(unsigned char *name, int flags, int mode, off_t siz)
{
	/* This is good for OS/2, where this will prevent file fragmentation,
	 * preallocating the desired file size upon open(). */
	return open(name, flags | O_SIZE, mode, (unsigned long) siz);
}

void
prealloc_truncate(int h, off_t siz)
{
	ftruncate(h, siz);
}
#endif
