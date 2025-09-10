#ifdef __DJGPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#define DOS_OVERRIDES_SELF

#include <pc.h>
#include <dpmi.h>
#include <bios.h>
#include <go32.h>
#include <libc/dosio.h>
#include <sys/exceptn.h>
#include <sys/movedata.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <values.h>
#ifdef CONFIG_QUICKJS
#include "js/quickjs/heartbeat.h"
#endif
#include "intl/libintl.h"
#include "main/main.h"
#include "main/select.h"
#include "network/connection.h"
#include "osdep/dos/dos.h"
#include "terminal/event.h"
#include "util/error.h"
#include "util/memory.h"

#define VIRTUAL_PIPE_SIZE	512

#define DOS_MOUSE_FLUSH_TIME	300

//#define TRACE_PIPES 1

#undef read
#undef write
#undef pipe
#undef close
#undef select

#ifdef HAVE_LONG_LONG
typedef unsigned long long uttime;
typedef unsigned long long tcount;
#else
typedef unsigned long uttime;
typedef unsigned long tcount;
#endif

#define DUMMY ((void *)-1L)

#ifdef __ICC
/* ICC OpenMP bug */
#define overalloc_condition     0
#else
#define overalloc_condition     1
#endif

#define overalloc_at(f, l)                                              \
do {                                                                    \
	EL_ERROR("ERROR: attempting to allocate too large block at %s:%d", f, l);\
	fflush(stdout); \
	fflush(stderr); \
	exit(RET_FATAL); \
} while (overalloc_condition)   /* while (1) is not a typo --- it's here to allow the compiler
        that doesn't know that fatal_exit doesn't return to do better
        optimizations */

#define overalloc()     overalloc_at(__FILE__, __LINE__)

int
is_xterm(void)
{
	ELOG
	return 0;
}

int
get_system_env(void)
{
	ELOG
	return (0);
}

int
set_nonblocking_fd(int fd)
{
	ELOG
#ifdef O_NONBLOCK
	int rs;
	EINTRLOOP(rs, fcntl(fd, F_SETFL, O_NONBLOCK));
#elif defined(FIONBIO)
	int rs;
	int on = 1;
	EINTRLOOP(rs, ioctl(fd, FIONBIO, &on));
#endif
	return 0;
}

static uttime
get_absolute_time(void)
{
	ELOG
	struct timeval tv;
	int rs;
	EINTRLOOP(rs, gettimeofday(&tv, NULL));

	if (rs) {
		EL_ERROR("gettimeofday failed: %d", errno);
		fflush(stdout);
		fflush(stderr);
		exit(RET_FATAL);
	}

	return (uttime)tv.tv_sec * 1000 + (unsigned)tv.tv_usec / 1000;
}

static uttime
get_time(void)
{
	ELOG
#if defined(OS2) || defined(WIN)
	static unsigned last_tim = 0;
	static uttime add = 0;
	unsigned tim;
#if defined(OS2)
	int rc;
	rc = DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &tim, sizeof tim);
	if (rc) {
		EL_ERROR("DosQuerySysInfo failed: %d", rc);
		fflush(stdout);
		fflush(stderr);
		exit(RET_FATAL);
	}
#elif defined(WIN)
	tim = GetTickCount();
#endif
	if (tim < last_tim) {
		add += (uttime)1 << 31 << 1;
	}
	last_tim = tim;
	return tim | add;
#else
#if defined(HAVE_CLOCK_GETTIME) && defined(TIME_WITH_SYS_TIME) && (defined(CLOCK_MONOTONIC_RAW) || defined(CLOCK_MONOTONIC))
	struct timespec ts;
	int rs;
#if defined(CLOCK_MONOTONIC_RAW)
	EINTRLOOP(rs, clock_gettime(CLOCK_MONOTONIC_RAW, &ts));
	if (!rs) return (uttime)ts.tv_sec * 1000 + (unsigned)ts.tv_nsec / 1000000;
#endif
#if defined(CLOCK_MONOTONIC)
	EINTRLOOP(rs, clock_gettime(CLOCK_MONOTONIC, &ts));
	if (!rs) return (uttime)ts.tv_sec * 1000 + (unsigned)ts.tv_nsec / 1000000;
#endif
#endif
	return get_absolute_time();
#endif
}

#define fd_lock()       do { } while (0)
#define fd_unlock()     do { } while (0)

#ifndef SA_RESTART
#define SA_RESTART 0
#endif

static void
new_fd_cloexec(int fd)
{
	ELOG
	int rs;
	EINTRLOOP(rs, fcntl(fd, F_SETFD, FD_CLOEXEC));
}

static int
cleanup_fds(void)
{
	ELOG
#ifdef ENFILE
	if (errno == ENFILE) {
		abort_background_connections();
		return 0;
	}
#endif
#ifdef EMFILE
	if (errno == EMFILE) {
		abort_background_connections();
		return 0;
	}
#endif
	return 0;
}

static int
c_socket(int d, int t, int p)
{
	ELOG
	int h;
	do {
		fd_lock();
		EINTRLOOP(h, socket(d, t, p));
		if (h != -1) new_fd_cloexec(h);
		fd_unlock();
	} while (h == -1 && cleanup_fds());
	return h;
}


/*
 * Asynchronous exits from signal handler cause a crash if they interrupt
 * networking in a wrong place. So, we use synchronous exit.
 */
static volatile unsigned char break_pressed = 0;
static volatile unsigned char break_exiting = 0;

void dos_poll_break(void)
{
	ELOG
	if (break_pressed && !break_exiting) {
		break_exiting = 1;
		EL_ERROR("Exiting on Ctrl+Break");
		fflush(stdout);
		fflush(stderr);
		exit(RET_FATAL);
	}
}

static void sigbreak(int sig)
{
	ELOG
	break_pressed = 1;
}

void get_terminal_size(int fd, int *x, int *y, int *cw, int *ch)
{
	ELOG
	*x = ScreenCols();
	*y = ScreenRows();
	*cw = 8;
	*ch = 16;
}

void handle_terminal_resize(int fd, void (*fn)(void))
{
	ELOG
}

void
unhandle_terminal_resize(int fd)
{
	ELOG
}

static size_t init_seq_len;

static unsigned char screen_initialized = 0;
static unsigned char *screen_backbuffer = NULL;
static int screen_backbuffer_x;
static int screen_backbuffer_y;
static int saved_cursor_x;
static int saved_cursor_y;
static unsigned char screen_default_attr;

static unsigned cursor_x;
static unsigned cursor_y;
static unsigned current_attr;

static unsigned char dos_mouse_initialized = 0;
static unsigned char dos_mouse_buttons;

static int dos_mouse_last_x;
static int dos_mouse_last_y;
static int dos_mouse_last_button;
static uttime dos_mouse_time;

static struct interlink_event *dos_mouse_queue = NULL;
static int dos_mouse_queue_n;

static void (*txt_mouse_handler)(void *, char *, int) = NULL;
static void *txt_mouse_data;

static int dos_mouse_coord(int v)
{
	ELOG
#if 0
	if (!F) v /= 8;
#endif
	return v / 8;
}

static void dos_mouse_show(void)
{
	ELOG
	if (dos_mouse_initialized) { // && !F) {
		__dpmi_regs r;
		memset(&r, 0, sizeof r);
		r.x.ax = 1;
		__dpmi_int(0x33, &r);
	}
}

static void dos_mouse_hide(void)
{
	ELOG
	if (dos_mouse_initialized) { // && !F) {
		__dpmi_regs r;
		memset(&r, 0, sizeof r);
		r.x.ax = 2;
		__dpmi_int(0x33, &r);
	}
}

static void dos_mouse_init(unsigned x, unsigned y)
{
	ELOG
	__dpmi_regs r;
	memset(&r, 0, sizeof r);
	__dpmi_int(0x33, &r);
	if (r.x.ax != 0xffff) return;
	dos_mouse_buttons = r.x.bx == 3 ? 3 : 2;
	dos_mouse_initialized = 1;
	memset(&r, 0, sizeof r);
	r.x.ax = 7;
	r.x.cx = 0;
	r.x.dx = x - 1;
	__dpmi_int(0x33, &r);
	memset(&r, 0, sizeof r);
	r.x.ax = 8;
	r.x.cx = 0;
	r.x.dx = y - 1;
	__dpmi_int(0x33, &r);
	memset(&r, 0, sizeof r);
	r.x.ax = 3;
	__dpmi_int(0x33, &r);
	dos_mouse_last_x = dos_mouse_coord(r.x.cx);
	dos_mouse_last_y = dos_mouse_coord(r.x.dx);
	dos_mouse_last_button = r.x.bx;
	dos_mouse_time = get_time();
}

void dos_mouse_terminate(void)
{
	ELOG
	mem_free_set(&dos_mouse_queue, NULL);
	dos_mouse_queue_n = 0;
	dos_mouse_hide();
}

static void dos_mouse_enqueue(int x, int y, int b)
{
	ELOG
	if (dos_mouse_queue_n && ((b & BM_ACT) == B_DRAG || (b & BM_ACT) == B_MOVE) && (b & BM_ACT) == (dos_mouse_queue[dos_mouse_queue_n - 1].info.mouse.button & BM_ACT)) {
		dos_mouse_queue_n--;
		goto set_last;
	}
	if ((unsigned)dos_mouse_queue_n > (unsigned)MAXINT / sizeof(struct interlink_event) - 1) {
		overalloc();
	}
	if (dos_mouse_queue == NULL) {
		dos_mouse_queue = (struct interlink_event *)mem_alloc((dos_mouse_queue_n + 1) * sizeof(struct interlink_event));
	} else {
		dos_mouse_queue = (struct interlink_event *)mem_realloc(dos_mouse_queue, (dos_mouse_queue_n + 1) * sizeof(struct interlink_event));
	}
set_last:
	dos_mouse_queue[dos_mouse_queue_n].ev = EVENT_MOUSE;
	dos_mouse_queue[dos_mouse_queue_n].info.mouse.x = x;
	dos_mouse_queue[dos_mouse_queue_n].info.mouse.y = y;
	dos_mouse_queue[dos_mouse_queue_n].info.mouse.button = b;
	dos_mouse_queue_n++;
}

static int dos_mouse_button(int b)
{
	ELOG
	switch (b) {
		default:
		case 0:	return B_LEFT;
		case 1: return B_RIGHT;
		case 2: return B_MIDDLE;
	}
}

static void dos_mouse_poll(void)
{
	ELOG
	int i;
	int cx, cy;
	__dpmi_regs r;
	dos_poll_break();
	if (dos_mouse_initialized) {
		if (dos_mouse_initialized == 1 && get_time() - dos_mouse_time > DOS_MOUSE_FLUSH_TIME)
			dos_mouse_initialized = 2;
		for (i = 0; i < dos_mouse_buttons; i++) {
			if (dos_mouse_initialized == 1 && i > 0) {
				memset(&r, 0, sizeof r);
				r.x.ax = 5;
				r.x.bx = i;
				__dpmi_int(0x33, &r);
				memset(&r, 0, sizeof r);
				r.x.ax = 6;
				r.x.bx = i;
				__dpmi_int(0x33, &r);
				continue;
			}
			memset(&r, 0, sizeof r);
			r.x.ax = !(dos_mouse_last_button & (1 << i)) ? 5 : 6;
			r.x.bx = i;
			__dpmi_int(0x33, &r);
px:
			if (r.x.bx) {
				dos_mouse_last_x = dos_mouse_coord(r.x.cx);
				dos_mouse_last_y = dos_mouse_coord(r.x.dx);
				dos_mouse_last_button ^= 1 << i;
				dos_mouse_enqueue(dos_mouse_last_x, dos_mouse_last_y, dos_mouse_button(i) | (dos_mouse_last_button & (1 << i) ? B_DOWN : B_UP));
				/*printf("%s %d %d\n", dos_mouse_last_button & (1 << i) ? "press" : "release", r.x.cx, r.x.dx);*/
				if (!((dos_mouse_last_button ^ r.x.ax) & (1 << i))) {
					memset(&r, 0, sizeof r);
					r.x.ax = !(dos_mouse_last_button & (1 << i)) ? 5 : 6;
					r.x.bx = i;
					__dpmi_int(0x33, &r);
					if ((dos_mouse_last_button ^ r.x.ax) & (1 << i))
						goto px;
				}
			}
		}
		memset(&r, 0, sizeof r);
		r.x.ax = 3;
		__dpmi_int(0x33, &r);
		cx = dos_mouse_coord(r.x.cx);
		cy = dos_mouse_coord(r.x.dx);
		if (r.h.bh) {
			dos_mouse_enqueue(cx, cy, (char)r.h.bh < 0 ? (B_DOWN | B_WHEEL_UP) : (B_DOWN | B_WHEEL_DOWN));
			goto x;
		}
		if (cx != dos_mouse_last_x || cy != dos_mouse_last_y) {
			for (i = 0; i < dos_mouse_buttons; i++)
				if (dos_mouse_last_button & (1 << i)) {
					dos_mouse_enqueue(cx, cy, B_DRAG | dos_mouse_button(i));
					goto x;
				}
//			if (F) dos_mouse_enqueue(cx, cy, B_MOVE);
x:
			dos_mouse_last_x = cx;
			dos_mouse_last_y = cy;
		}
	}
#if defined(G) && defined(GRDRV_GRX)
	if (grx_mouse_initialized) {
		grx_mouse_poll();
	}
#endif
}

void *handle_mouse(int cons, void (*fn)(void *, char *, int), void *data)
{
	ELOG
	int x, y, cw, ch;
	get_terminal_size(cons, &x, &y, &cw, &ch);
	dos_mouse_init(x * 8, y * 8);

	if (!dos_mouse_initialized) return NULL;
	dos_mouse_show();
	txt_mouse_handler = fn;
	txt_mouse_data = data;

	return DUMMY;
}

void unhandle_mouse(void *data)
{
	ELOG
	dos_mouse_terminate();
	txt_mouse_handler = NULL;
}

void
suspend_mouse(void *data)
{
	ELOG
}

void
resume_mouse(void *data)
{
	ELOG
}

void want_draw(void)
{
	ELOG
	dos_mouse_hide();
}

void done_draw(void)
{
	ELOG
	dos_mouse_show();
}

static int dos_mouse_event(void)
{
	ELOG
	if (dos_mouse_queue_n) {
		if (/*!F &&*/ txt_mouse_handler) {
			struct interlink_event *q = dos_mouse_queue;
			int ql = dos_mouse_queue_n;
			dos_mouse_queue = NULL;
			dos_mouse_queue_n = 0;
			txt_mouse_handler(txt_mouse_data, (char *)(void *)q, ql * sizeof(struct interlink_event));
			mem_free(q);
			return 1;
		}
	}
#if defined(G) && defined(GRDRV_GRX)
	if (grx_mouse_initialized) {
		return grx_mouse_event();
	}
#endif
	return 0;
}

void save_terminal(void)
{
	ELOG
	unsigned char *sc;
	want_draw();
	screen_backbuffer_x = ScreenCols();
	screen_backbuffer_y = ScreenRows();
	screen_default_attr = ScreenAttrib;
	if (screen_backbuffer) {
		sc = screen_backbuffer;
		screen_backbuffer = NULL;
		mem_free(sc);
	}
	sc = (unsigned char *)mem_alloc(2 * screen_backbuffer_x * screen_backbuffer_y);
	ScreenRetrieve(sc);
	ScreenGetCursor(&saved_cursor_y, &saved_cursor_x);
	screen_backbuffer = sc;
	done_draw();
}

void restore_terminal(void)
{
	ELOG
	want_draw();
	if (screen_backbuffer) {
		unsigned char *sc;
		if (ScreenCols() == screen_backbuffer_x && ScreenRows() == screen_backbuffer_y) {
			ScreenUpdate(screen_backbuffer);
			ScreenSetCursor(saved_cursor_y, saved_cursor_x);
		}
		sc = screen_backbuffer;
		screen_backbuffer = NULL;
		mem_free(sc);
	}
	done_draw();
}

static void ansi_initialize(void)
{
	ELOG
	if (screen_initialized)
		return;

	ScreenClear();
	cursor_x = 0;
	cursor_y = 0;
	current_attr = screen_default_attr;
	screen_initialized = 1;
}

static void ansi_terminate(void)
{
	ELOG
	if (!screen_initialized)
		return;

	ScreenSetCursor(0, 0);
	screen_initialized = 0;
}

static unsigned ansi2pc(unsigned c)
{
	ELOG
	return ((c & 4) >> 2) | (c & 2) | ((c & 1) << 2);
}

static unsigned char init_seq[] = "\033)0\0337";

static inline
unsigned upcase(unsigned a)
{
	ELOG
	if (a >= 'a' && a <= 'z') a -= 0x20;
	return a;
}

static inline
unsigned locase(unsigned a)
{
	ELOG
	if (a >= 'A' && a <= 'Z') a += 0x20;
	return a;
}
static inline
int srch_cmp(unsigned char c1, unsigned char c2)
{
	ELOG
	return upcase(c1) != upcase(c2);
}

static void ansi_write(const unsigned char *str, size_t size)
{
	ELOG
	if (!screen_initialized) {
		if (size >= init_seq_len && !memcmp(str, init_seq, init_seq_len)) {
			ansi_initialize();
			goto process_ansi;
		}
		write(1, str, size);
		return;
	}
process_ansi:
	while (size--) {
		unsigned char c = *str++;
		unsigned char buffer[128];
		unsigned buf_size;
		unsigned clen;
		unsigned x, y;
		switch (c) {
			case 7:
				continue;
			case 10:
				cursor_y++;
				/*-fallthrough*/
			case 13:
				cursor_x = 0;
				break;
			case 27:
				if (!size--) goto ret;
				switch (*str++) {
					case ')':
						if (!size--) goto ret;
						str++;
						continue;
					case '7':
					default:
						continue;
					case '8':
						ansi_terminate();
						goto ret2;
					case '[':
						clen = 0;
						while (1) {
							unsigned char u;
							if (clen >= size) goto ret;
							u = upcase(str[clen]);
							if (u >= 'A' && u <= 'Z')
								break;
							clen++;
						}
				}
				if (clen >= sizeof(buffer)) goto ret;
				memcpy(buffer, str, clen);
				buffer[clen] = 0;
				switch (str[clen]) {
					case 'J':
						if (!strcmp((const char *) buffer, "2"))
							ScreenClear();
						break;
					case 'H':
						if (sscanf((const char *) buffer, "%u;%u", &y, &x) == 2) {
							cursor_x = x - 1;
							cursor_y = y - 1;
						}
						break;
					case 'm':
						while (buffer[0] >= '0' && buffer[0] < '9') {
							unsigned long a;
							char *e;
							a = strtoul((const char *) buffer, &e, 10);
							if (*e == ';') e++;
							memmove(buffer, e, strlen((const char *) e) + 1);
							switch (a) {
								case 0:
									current_attr = screen_default_attr;
									break;
								case 1:
									current_attr |= 0x08;
									break;
								case 7:
									current_attr = ((screen_default_attr & 0x70) >> 4) | ((screen_default_attr & 0x07) << 4);
									break;
								case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
									current_attr = (current_attr & 0x78) | ansi2pc(a - 30);
									break;
								case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
									current_attr = (current_attr & 0x0f) | (ansi2pc(a - 40) << 4);
									break;
							}
						}
						break;
				}
				str += clen + 1;
				size -= clen + 1;
				continue;
			default:
				buffer[0] = c;
				buf_size = 1;
				while (size && *str >= ' ' && buf_size < sizeof(buffer) - 1) {
					buffer[buf_size++] = *str++;
					size--;
				}
				buffer[buf_size] = 0;
				ScreenPutString((const char *) buffer, current_attr, cursor_x, cursor_y);
				cursor_x += buf_size;
				break;
		}
	}
ret:
	ScreenSetCursor(cursor_y, cursor_x);
ret2:;
}

#ifdef DOS_EXTRA_KEYBOARD

static short dos_buffered_char = -1;

static int dos_select_keyboard(void)
{
	ELOG
	int bk;
	if (dos_buffered_char >= 0) return 1;
	bk = bioskey(0x11);
	return !!bk;
}

static int dos_read_keyboard(void *buf, size_t size)
{
	ELOG
	int k;
	if (!size) return 0;
	if (dos_buffered_char >= 0) {
		*(unsigned char *)buf = dos_buffered_char;
		dos_buffered_char = -1;
		return 1;
	}
	k = getkey();
	if (!k) return 0;
	/*printf("returned key %04x\n", k);*/
	if (k < 0x100) {
		*(unsigned char *)buf = k;
		return 1;
	} else {
		*(unsigned char *)buf = 0;
		if (size >= 2) {
			*((unsigned char *)buf + 1) = k;
			return 2;
		} else {
			dos_buffered_char = k & 0xff;
			return 1;
		}
	}
}

#endif

static inline void pipe_lock(void)
{
	ELOG
}

static inline void pipe_unlock(void)
{
	ELOG
}

static inline void pipe_unlock_wait(void)
{
	ELOG
}

static inline void pipe_wake(void)
{
	ELOG
}

#include "vpipe.inc"

int dos_pipe(int fd[2])
{
	ELOG
	int r = vpipe_create(fd);
	/*printf("dos_pipe: (%d) : %d,%d\n", r, fd[0], fd[1]);*/
	return r;
}

int dos_read(int fd, void *buf, size_t size)
{
	ELOG
	int r;
	dos_mouse_poll();
	r = vpipe_read(fd, buf, size);
	/*printf("dos_read(%d,%d) : %d,%d\n", r, errno, fd, size);*/
	if (r != -2) return r;
#ifdef DOS_EXTRA_KEYBOARD
	if (fd == 0) return dos_read_keyboard(buf, size);
#endif
	return read(fd, buf, size);
}

int dos_write(int fd, const void *buf, size_t size)
{
	ELOG
	int r;
	dos_mouse_poll();
	r = vpipe_write(fd, buf, size);
	/*printf("dos_write(%d,%d) : %d,%d\n", r, errno, fd, size);*/
	if (r != -2) return r;
	if (fd == 1) {
		ansi_write((const unsigned char *)buf, size);
		return size;
	}
	return write(fd, buf, size);
}

int dos_close(int fd)
{
	ELOG
	int r;
	r = vpipe_close(fd);
	if (r != -2) return r;
	return close(fd);
}

int dos_select(int n, fd_set *rs, fd_set *ws, fd_set *es, struct timeval *t, int from_main_loop)
{
	ELOG
	int i;
	int last_pass = 0;
	int ret_cnt = 0;

	int r;
	fd_set rsb, wsb, esb;
	uclock_t start, tt;

	if (t) {
		start = uclock();
		tt = (t->tv_sec + (t->tv_usec / 1000000.0)) * UCLOCKS_PER_SEC;
	}

	dos_mouse_poll();

	pipe_lock();
	for (i = 0; i < n; i++) {
		int ts = 0;
		if (rs && FD_ISSET(i, rs)) {
			int signaled = 0;
			if (pipe_desc[i]) {
				if (vpipe_may_read(i))
					signaled = 1;
				else
					FD_CLR(i, rs);
				/*printf("dos_select_read(%d) : %d\n", i, vpipe_may_read(i));*/
			} else {
			}
			if (!last_pass) {
				if (signaled) {
					clear_inactive(rs, i);
					clear_inactive(ws, i);
					clear_inactive(es, i);
					last_pass = 1;
				}
			} else {
				if (!signaled) FD_CLR(i, rs);
			}
			ts |= signaled;
		}
		if (ws && FD_ISSET(i, ws)) {
			int signaled = 0;
			if (pipe_desc[i]) {
				if (vpipe_may_write(i))
					signaled = 1;
				else
					FD_CLR(i, ws);
				/*printf("dos_select_write(%d) : %d\n", i, vpipe_may_write(i));*/
			} else {
			}
			if (!last_pass) {
				if (signaled) {
					clear_inactive(rs, i + 1);
					clear_inactive(ws, i);
					clear_inactive(es, i);
					last_pass = 1;
				}
			} else {
				if (!signaled) FD_CLR(i, ws);
			}
			ts |= signaled;
		}
		if (es && FD_ISSET(i, es)) {
			int signaled = 0;
			if (pipe_desc[i]) {
				FD_CLR(i, es);
			} else {
			}
			if (!last_pass) {
				if (signaled) {
					clear_inactive(rs, i + 1);
					clear_inactive(ws, i + 1);
					clear_inactive(es, i);
					last_pass = 1;
				}
			} else {
				if (!signaled) FD_CLR(i, es);
			}
			ts |= signaled;
		}
		if (last_pass) ret_cnt += ts;
	}
	pipe_unlock();
	if (last_pass) {
		/*printf("ret_cnt: %d\n", ret_cnt);*/
		return ret_cnt;
	}
	/*printf("real select\n");*/
	/*return select(n, rs, ws, es, t);*/
	if (rs) rsb = *rs;
	else FD_ZERO(&rsb);
	if (ws) wsb = *ws;
	else FD_ZERO(&wsb);
	if (es) esb = *es;
	else FD_ZERO(&esb);

	while (1) {
		struct timeval zero = { 0, 0 };
		struct timeval now;
#ifdef DOS_EXTRA_KEYBOARD
		if (rs && FD_ISSET(0, rs)) {
			if (dos_select_keyboard()) {
				FD_ZERO(rs);
				FD_SET(0, rs);
				if (ws) FD_ZERO(ws);
				if (es) FD_ZERO(es);
				if (from_main_loop && dos_mouse_event())
					check_bottom_halves();
				return 1;
			}
			FD_CLR(0, rs);
		}
#endif
		if (from_main_loop && dos_mouse_event()) {
			check_bottom_halves();
			return 0;
		}
		r = select(n, rs, ws, es, &zero);
		if (r == -1 && errno == EBADF) {
			/* DJGPP occasionally returns EBADF */
			int re = 0;
			if (rs) FD_ZERO(rs);
			if (ws) FD_ZERO(ws);
			if (es) FD_ZERO(es);
			i = 0;
#ifdef DOS_EXTRA_KEYBOARD
			i++;
#endif
			for (; i < n; i++) {
				fd_set rsx;
				fd_set wsx;
				fd_set esx;
				int x, s;
				if (!FD_ISSET(i, &rsb) &&
				    !FD_ISSET(i, &wsb) &&
				    !FD_ISSET(i, &esb))
					continue;
				FD_ZERO(&rsx);
				FD_ZERO(&wsx);
				FD_ZERO(&esx);
				if (FD_ISSET(i, &rsb)) FD_SET(i, &rsx);
				if (FD_ISSET(i, &wsb)) FD_SET(i, &wsx);
				if (FD_ISSET(i, &esb)) FD_SET(i, &esx);
				zero.tv_sec = 0;
				zero.tv_usec = 0;
				x = select(i + 1, &rsx, &wsx, &esx, &zero);
				s = 0;
				if (FD_ISSET(i, &rsb) && (x < 0 || (x > 0 && FD_ISSET(i, &rsx)))) FD_SET(i, rs), s = 1;
				if (FD_ISSET(i, &wsb) && (x < 0 || (x > 0 && FD_ISSET(i, &wsx)))) FD_SET(i, ws), s = 1;
				if (FD_ISSET(i, &esb) && (x < 0 || (x > 0 && FD_ISSET(i, &esx)))) FD_SET(i, es), s = 1;
				if (s) re++;
			}
			if (re) return re;
			r = 0;
		}
		if (r) return r;
		if (t) {
			if (uclock() - start >= tt) {
				return 0;
			}
		}
		if (rs) *rs = rsb;
		if (ws) *ws = wsb;
		if (es) *es = esb;
		dos_mouse_poll();
		__dpmi_yield();
		dos_mouse_poll();
	}
}

long
os_get_free_mem_in_mib(void)
{
	ELOG
	__dpmi_memory_info buffer;
	int ret = __dpmi_get_memory_information(&buffer);

	if (ret) {
		__dpmi_free_mem_info info;
		__dpmi_get_free_memory_information(&info);

		return info.total_number_of_free_pages / 256;
	}
	return buffer.total_available_bytes_of_virtual_memory_client / (1024 * 1024);
}

#ifdef DOS_EXTRA_KEYBOARD

int dos_setraw(int ctl, int save)
{
	ELOG
	__djgpp_set_ctrl_c(0);
	return 0;
}

void setcooked(int ctl)
{
	ELOG
}

#endif

#define RANDOM_POOL_SIZE	65536

void os_seed_random(unsigned char **pool, int *pool_size)
{
	ELOG
	unsigned *random_pool, *tmp_pool;
	int a, i;
	random_pool = (unsigned *)mem_alloc(RANDOM_POOL_SIZE);
	tmp_pool = (unsigned *)mem_alloc(RANDOM_POOL_SIZE);
	for (a = 0; a <= 640 * 1024 - RANDOM_POOL_SIZE; a += RANDOM_POOL_SIZE) {
		dosmemget(a, RANDOM_POOL_SIZE, tmp_pool);
		for (i = 0; i < RANDOM_POOL_SIZE / 4; i++)
			random_pool[i] += tmp_pool[i];
	}
	mem_free(tmp_pool);
	*pool = (unsigned char *)(void *)random_pool;
	*pool_size = RANDOM_POOL_SIZE;
}

#ifdef CONFIG_QUICKJS
_go32_dpmi_seginfo OldISR, NewISR;
#define TIMER 8
//Simple Example of chaining interrupt handlers
//Adopted from Matthew Mastracci's code
//macros by Shawn Hargreaves from the Allegro library for locking
//functions and variables.
#define LOCK_VARIABLE(x)    _go32_dpmi_lock_data((void *)&x,(long)sizeof(x));
#define LOCK_FUNCTION(x)    _go32_dpmi_lock_code(x,(long)sizeof(x));

static void
TickHandler(void)
{
	ELOG
	static int internal = 0;

	if (internal++ >= 19) {
		internal = 0;
		check_heartbeats(NULL);
	}
}
#endif

void init_osdep(void)
{
	ELOG
	int s, rs;
	struct sigaction sa;

#ifdef _STAT_INODE
	_djstat_flags |= _STAT_INODE;
#endif
#ifdef _STAT_EXEC_EXT
	_djstat_flags |= _STAT_EXEC_EXT;
#endif
#ifdef _STAT_EXEC_MAGIC
	_djstat_flags |= _STAT_EXEC_MAGIC;
#endif

	init_seq_len = strlen((const char *) init_seq);

	/* preload the packet driver */
	s = c_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s >= 0) {
		EINTRLOOP(rs, close(s));
	}

	//tcp_cbreak(1);

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sigbreak;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	EINTRLOOP(rs, sigaction(SIGINT, &sa, NULL));

#ifdef CONFIG_QUICKJS
	LOCK_FUNCTION(TickHandler);
	LOCK_FUNCTION(check_heartbeats);
//load the address of the old timer ISR into the OldISR structure
	_go32_dpmi_get_protected_mode_interrupt_vector(TIMER, &OldISR);
//point NewISR to the proper selector:offset for handler
//function
	NewISR.pm_offset = (int)TickHandler;
	NewISR.pm_selector = _go32_my_cs();
//chain the new ISR onto the old one so that first the old
//timer ISR
//will be called, then the new timer ISR
	_go32_dpmi_chain_protected_mode_interrupt_vector(TIMER, &NewISR);
#endif
}

void terminate_osdep(void)
{
	ELOG
	if (screen_backbuffer)
		mem_free(screen_backbuffer);
#ifdef CONFIG_QUICKJS
	_go32_dpmi_set_protected_mode_interrupt_vector(TIMER, &OldISR);
#endif
}

#define LINKS_BIN_SEARCH(entries, eq, ab, key, result)                        \
{                                                                       \
	int s_ = 0, e_ = (entries) - 1;                                 \
	(result) = -1;                                                  \
	while (s_ <= e_) {                                              \
		int m_ = (int)(((unsigned)s_ + (unsigned)e_) / 2);      \
		if (eq((m_), (key))) {                                  \
			(result) = m_;                                  \
			break;                                          \
		}                                                       \
		if (ab((m_), (key))) e_ = m_ - 1;                       \
		else s_ = m_ + 1;                                       \
	}                                                               \
}

#define array_elements(a)       (sizeof(a) / sizeof(*a))

int
get_country_language(int c)
{
	ELOG
	static const struct {
		int code;
		const char *language;
	} countries[] = {
		{ 1,  "English" },
		{ 2,  "French" },
		{ 3,  "Spanish" },
		{ 4,  "English" },
		{ 7,  "Russian" },
		{ 27,  "English" },
		{ 30,  "Greek" },
		{ 31,  "Dutch" },
		{ 32,  "Dutch" },
		{ 33,  "French" },
		{ 34,  "Spanish" },
		{ 36,  "Hungarian" },
		{ 38,  "Serbian" },
		{ 39,  "Italian" },
		{ 40,  "Romanian" },
		{ 41,  "Swiss German" },
		{ 42,  "Czech" },
		{ 43,  "German" },
		{ 44,  "English" },
		{ 45,  "Danish" },
		{ 46,  "Swedish" },
		{ 47,  "Norwegian" },
		{ 48,  "Polish" },
		{ 49,  "German" },
		{ 52,  "Spanish" },
		{ 54,  "Spanish" },
		{ 55,  "Brazilian Portuguese" },
		{ 56,  "Spanish" },
		{ 57,  "Spanish" },
		{ 58,  "Spanish" },
		{ 61,  "English" },
		{ 64,  "English" },
		{ 65,  "English" },
		{ 90,  "Turkish" },
		{ 99,  "English" },
		{ 351,  "Portuguese" },
		{ 353,  "English" },
		{ 354,  "Icelandic" },
		{ 358,  "Finnish" },
		{ 359,  "Bulgarian" },
		{ 371,  "Lithuanian" },
		{ 372,  "Estonian" },
		{ 381,  "Serbian" },
		{ 384,  "Croatian" },
		{ 385,  "Croatian" },
#ifdef DOS
		{ 421,  "Slovak" },
#else
		{ 421,  "Czech" },
#endif
		{ 422,  "Slovak" },
		{ 593,  "Spanish" },
	};
	int idx = -1;
#define C_EQUAL(a, b)   countries[a].code == (b)
#define C_ABOVE(a, b)   countries[a].code > (b)

#if !defined(CONFIG_NLS) && !defined(CONFIG_GETTEXT)
	return 1;
#else
	LINKS_BIN_SEARCH(array_elements(countries), C_EQUAL, C_ABOVE, c, idx);
	if (idx == -1)
		return 1;
	return name_to_language(countries[idx].language);
#endif
}

int os_default_language(void)
{
	ELOG
	__dpmi_regs r;
	memset(&r, 0, sizeof r);
	r.x.ax = 0x3800;
	r.x.dx = __tb_offset;
	r.x.ds = __tb_segment;
	__dpmi_int(0x21, &r);
	if (!(r.x.flags & 1)) {
		return get_country_language(r.x.bx);
	}
	return -1;
}

int os_default_charset(void)
{
	ELOG
	__dpmi_regs r;
	memset(&r, 0, sizeof r);
	r.x.ax = 0x6601;
	__dpmi_int(0x21, &r);
	if (!(r.x.flags & 1)) {
		char a[8];
		int cp;
		snprintf((char *) a, sizeof a, "%d", r.x.bx);
		if ((cp = get_cp_index(a)) >= 0 && !is_cp_utf8(cp))
			return cp;
	}
	return -1;
}

int dos_is_bw(void)
{
	ELOG
	unsigned char cfg;
	dosmemget(0x410, sizeof cfg, &cfg);
	return (cfg & 0x30) == 0x30;
}

#else

typedef int dos_c_no_empty_unit;

#endif
