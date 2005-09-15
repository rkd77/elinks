/* Support for keyboard interface */
/* $Id: kbd.c,v 1.112.6.8 2005/09/14 12:14:22 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef __hpux__
#include <limits.h>
#define HPUX_PIPE	(len > PIPE_BUF || errno != EAGAIN)
#else
#define HPUX_PIPE	1
#endif

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define OUT_BUF_SIZE	16384
#define IN_BUF_SIZE	16

#define TW_BUTT_LEFT	1
#define TW_BUTT_MIDDLE	2
#define TW_BUTT_RIGHT	4

struct itrm {
	int std_in;
	int std_out;
	int sock_in;
	int sock_out;
	int ctl_in;

	/* Input queue */
	unsigned char kqueue[IN_BUF_SIZE];
	int qlen;

	/* Output queue */
	unsigned char *ev_queue;
	int eqlen;

	int timer;			/* ESC timeout timer */
	struct termios t;		/* For restoring original attributes */
	void *mouse_h;			/* Mouse handle */
	unsigned char *orig_title;	/* For restoring window title */

	unsigned int blocked:1;		/* Whether it was blocked */
	unsigned int altscreen:1;	/* Whether to use alternate screen */
	unsigned int touched_title:1;	/* Whether the term title was changed */
};

static struct itrm *ditrm = NULL;

static void free_trm(struct itrm *);
static void in_kbd(struct itrm *);
static void in_sock(struct itrm *);
static int process_queue(struct itrm *);

int
is_blocked(void)
{
	return ditrm && ditrm->blocked;
}


void
free_all_itrms(void)
{
	if (ditrm) free_trm(ditrm);
}


static void
write_ev_queue(struct itrm *itrm)
{
	int written;
	int qlen = int_min(itrm->eqlen, 128);

	assertm(qlen, "event queue empty");
	if_assert_failed return;

	written = safe_write(itrm->sock_out, itrm->ev_queue, qlen);
	if (written <= 0) {
		if (written < 0) free_trm(itrm); /* write error */
		return;
	}

	itrm->eqlen -= written;

	if (itrm->eqlen == 0) {
		set_handlers(itrm->sock_out,
			     get_handler(itrm->sock_out, H_READ),
			     NULL,
			     get_handler(itrm->sock_out, H_ERROR),
			     get_handler(itrm->sock_out, H_DATA));
	} else {
		assert(itrm->eqlen > 0);
		memmove(itrm->ev_queue, itrm->ev_queue + written, itrm->eqlen);
	}
}


static void
queue_event(struct itrm *itrm, unsigned char *data, int len)
{
	int w = 0;

	if (!len) return;

	if (!itrm->eqlen && can_write(itrm->sock_out)) {
		w = safe_write(itrm->sock_out, data, len);
		if (w <= 0 && HPUX_PIPE) {
			/* free_trm(itrm); */
			register_bottom_half((void (*)(void *)) free_trm, itrm);
			return;
		}
	}

	if (w < len) {
		int left = len - w;
		unsigned char *c = mem_realloc(itrm->ev_queue,
					       itrm->eqlen + left);

		if (!c) {
			free_trm(itrm);
			return;
		}

		itrm->ev_queue = c;
		memcpy(itrm->ev_queue + itrm->eqlen, data + w, left);
		itrm->eqlen += left;
		set_handlers(itrm->sock_out,
			     get_handler(itrm->sock_out, H_READ),
			     (void (*)(void *)) write_ev_queue,
			     (void (*)(void *)) free_trm, itrm);
	}
}


void
kbd_ctrl_c(void)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, KBD_CTRL_C, 0, 0);

	if (!ditrm) return;
	queue_event(ditrm, (unsigned char *) &ev, sizeof(ev));
}

#define write_sequence(fd, seq) \
	hard_write(fd, seq, sizeof(seq) / sizeof(unsigned char) - 1)

#define INIT_TERMINAL_SEQ	"\033)0\0337"	/* Special Character and Line Drawing Set, Save Cursor */
#define INIT_TWIN_MOUSE_SEQ	"\033[?9h"	/* Send MIT Mouse Row & Column on Button Press */
#define INIT_XWIN_MOUSE_SEQ	"\033[?1000h"	/* Send Mouse X & Y on button press and release */
#define INIT_ALT_SCREEN_SEQ	"\033[?47h"	/* Use Alternate Screen Buffer */

static void
send_init_sequence(int h, int altscreen)
{
	write_sequence(h, INIT_TERMINAL_SEQ);

	/* If alternate screen is supported switch to it. */
	if (altscreen) {
		write_sequence(h, INIT_ALT_SCREEN_SEQ);
	}
#ifdef CONFIG_MOUSE
	write_sequence(h, INIT_TWIN_MOUSE_SEQ);
	write_sequence(h, INIT_XWIN_MOUSE_SEQ);
#endif
}

#define DONE_CLS_SEQ		"\033[2J"	/* Erase in Display, Clear All */
#define DONE_TERMINAL_SEQ	"\0338\r \b"	/* Restore Cursor (DECRC) + ??? */
#define DONE_TWIN_MOUSE_SEQ	"\033[?9l"	/* Don't Send MIT Mouse Row & Column on Button Press */
#define DONE_XWIN_MOUSE_SEQ	"\033[?1000l"	/* Don't Send Mouse X & Y on button press and release */
#define DONE_ALT_SCREEN_SEQ	"\033[?47l"	/* Use Normal Screen Buffer */

static void
send_done_sequence(int h, int altscreen)
{
	write_sequence(h, DONE_CLS_SEQ);

#ifdef CONFIG_MOUSE
	/* This is a hack to make xterm + alternate screen working,
	 * if we send only DONE_XWIN_MOUSE_SEQ, mouse is not totally
	 * released it seems, in rxvt and xterm... --Zas */
	write_sequence(h, DONE_TWIN_MOUSE_SEQ);
	write_sequence(h, DONE_XWIN_MOUSE_SEQ);
#endif

	/* Switch from alternate screen. */
	if (altscreen) {
		write_sequence(h, DONE_ALT_SCREEN_SEQ);
	}

	write_sequence(h, DONE_TERMINAL_SEQ);
}

#undef write_sequence

void
resize_terminal(void)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_RESIZE, 0, 0, 0);
	int width, height;

	get_terminal_size(ditrm->std_out, &width, &height);

	ev.info.size.width = width;
	ev.info.size.height = height;

	queue_event(ditrm, (char *) &ev, sizeof(ev));
}

static void
set_terminal_name(unsigned char name[MAX_TERM_LEN])
{
	unsigned char *term = getenv("TERM");
	int i;

	memset(name, 0, MAX_TERM_LEN);

	if (!term) return;

	for (i = 0; term[i] != 0 && i < MAX_TERM_LEN - 1; i++)
		name[i] = isident(term[i]) ? term[i] : '-';
}


static int
setraw(int fd, struct termios *p)
{
	struct termios t;

	memset(&t, 0, sizeof(t));
	if (tcgetattr(fd, &t)) return -1;

	if (p) copy_struct(p, &t);

	elinks_cfmakeraw(&t);
	t.c_lflag |= ISIG;
#ifdef TOSTOP
	t.c_lflag |= TOSTOP;
#endif
	t.c_oflag |= OPOST;
	if (tcsetattr(fd, TCSANOW, &t)) return -1;

	return 0;
}

void
handle_trm(int std_in, int std_out, int sock_in, int sock_out, int ctl_in,
	   void *init_string, int init_len, int remote)
{
	struct itrm *itrm;
	struct terminal_info info;
	struct term_event_size *size = &info.event.info.size;
	unsigned char *ts;

	memset(&info, 0, sizeof(info));

	get_terminal_size(ctl_in, &size->width, &size->height);
	info.event.ev = EVENT_INIT;
	info.system_env = get_system_env();
	info.length = init_len;

	if (remote) {
		info.session_info = remote;
		info.magic = INTERLINK_REMOTE_MAGIC;
	} else {
		info.session_info = get_cmd_opt_int("base-session");
		info.magic = INTERLINK_NORMAL_MAGIC;
	}

	itrm = mem_calloc(1, sizeof(*itrm));
	if (!itrm) return;

	ditrm = itrm;
	itrm->std_in = std_in;
	itrm->std_out = std_out;
	itrm->sock_in = sock_in;
	itrm->sock_out = sock_out;
	itrm->ctl_in = ctl_in;
	itrm->timer = -1;

	/* FIXME: Combination altscreen + xwin does not work as it should,
	 * mouse clicks are reportedly partially ignored. */
	if (info.system_env & (ENV_SCREEN | ENV_XWIN))
		itrm->altscreen = 1;

	if (ctl_in >= 0) setraw(ctl_in, &itrm->t);
	set_handlers(std_in, (void (*)(void *)) in_kbd,
		     NULL, (void (*)(void *)) free_trm, itrm);

	if (sock_in != std_out)
		set_handlers(sock_in, (void (*)(void *)) in_sock,
			     NULL, (void (*)(void *)) free_trm, itrm);

	handle_terminal_resize(ctl_in, resize_terminal);

	set_terminal_name(info.name);

	ts = get_cwd();
	if (ts) {
		memcpy(info.cwd, ts, int_min(strlen(ts), MAX_CWD_LEN));
		mem_free(ts);
	}

	queue_event(itrm, (char *) &info, TERMINAL_INFO_SIZE);
	queue_event(itrm, (char *) init_string, init_len);
	send_init_sequence(std_out, itrm->altscreen);

	itrm->mouse_h = handle_mouse(0, (void (*)(void *, unsigned char *, int)) queue_event, itrm);
}


static void
unblock_itrm_x(void *h)
{
	close_handle(h);
	if (!ditrm) return;
	unblock_itrm(0);
	resize_terminal();
}


int
unblock_itrm(int fd)
{
	struct itrm *itrm = ditrm;

	if (!itrm) return -1;

	if (itrm->ctl_in >= 0 && setraw(itrm->ctl_in, NULL)) return -1;
	itrm->blocked = 0;
	send_init_sequence(itrm->std_out, itrm->altscreen);

	set_handlers(itrm->std_in, (void (*)(void *)) in_kbd, NULL,
		     (void (*)(void *)) free_trm, itrm);

	resume_mouse(itrm->mouse_h);

	handle_terminal_resize(itrm->ctl_in, resize_terminal);
	unblock_stdin();

	return 0;
}


void
block_itrm(int fd)
{
	struct itrm *itrm = ditrm;

	if (!itrm) return;

	itrm->blocked = 1;
	block_stdin();
	unhandle_terminal_resize(itrm->ctl_in);
	send_done_sequence(itrm->std_out, itrm->altscreen);
	tcsetattr(itrm->ctl_in, TCSANOW, &itrm->t);
	set_handlers(itrm->std_in, NULL, NULL,
		     (void (*)(void *)) free_trm, itrm);
	suspend_mouse(itrm->mouse_h);
}


static void
free_trm(struct itrm *itrm)
{
	if (!itrm) return;

	if (itrm->orig_title && *itrm->orig_title) {
		set_window_title(itrm->orig_title);

	} else if (itrm->touched_title) {
		/* Set the window title to the value of $TERM if X11 wasn't
		 * compiled in. Should hopefully make at least half the users
		 * happy. (debian bug #312955) */
		unsigned char title[MAX_TERM_LEN];

		set_terminal_name(title);
		if (*title)
			set_window_title(title);
	}

	mem_free_set(&itrm->orig_title, NULL);

	unhandle_terminal_resize(itrm->ctl_in);
	unhandle_mouse(itrm->mouse_h);
	send_done_sequence(itrm->std_out,itrm->altscreen);
	tcsetattr(itrm->ctl_in, TCSANOW, &itrm->t);

	clear_handlers(itrm->std_in);
	clear_handlers(itrm->sock_in);
	clear_handlers(itrm->std_out);
	clear_handlers(itrm->sock_out);

	if (itrm->timer != -1)
		kill_timer(itrm->timer);

	if (itrm == ditrm) ditrm = NULL;
	mem_free_if(itrm->ev_queue);
	mem_free(itrm);
}

/* Resize terminal to dimensions specified by @text string.
 * @text should look like "width,height,old-width,old-height" where width and
 * height are integers. */
static inline void
resize_terminal_from_str(unsigned char *text)
{
	enum { NEW_WIDTH = 0, NEW_HEIGHT, OLD_WIDTH, OLD_HEIGHT, NUMBERS } i;
	int numbers[NUMBERS];

	assert(text && *text);
	if_assert_failed return;

	for (i = 0; i < NUMBERS; i++) {
		unsigned char *p = strchr(text, ',');

		if (p) {
			*p++ = '\0';

		} else if (i < OLD_HEIGHT) {
			return;
		}

		numbers[i] = atoi(text);

		if (p) text = p;
	}

	resize_window(numbers[NEW_WIDTH], numbers[NEW_HEIGHT],
		      numbers[OLD_WIDTH], numbers[OLD_HEIGHT]);
	resize_terminal();
}

void
dispatch_special(unsigned char *text)
{
	switch (text[0]) {
		case TERM_FN_TITLE:
			if (ditrm) {
				if (!ditrm->orig_title)
					ditrm->orig_title = get_window_title();
				ditrm->touched_title = 1;
			}
			set_window_title(text + 1);
			break;
		case TERM_FN_RESIZE:
			resize_terminal_from_str(text + 1);
			break;
	}
}

static void inline
safe_hard_write(int fd, unsigned char *buf, int len)
{
	if (is_blocked()) return;

	want_draw();
	hard_write(fd, buf, len);
	done_draw();
}

static void
in_sock(struct itrm *itrm)
{
	struct string path;
	struct string delete;
	char ch;
	int fg;
	int bytes_read, i, p;
	unsigned char buf[OUT_BUF_SIZE];

	bytes_read = safe_read(itrm->sock_in, buf, OUT_BUF_SIZE);
	if (bytes_read <= 0) goto free_and_return;

qwerty:
	for (i = 0; i < bytes_read; i++)
		if (!buf[i])
			goto has_nul_byte;

	safe_hard_write(itrm->std_out, buf, bytes_read);
	return;

has_nul_byte:
	if (i) safe_hard_write(itrm->std_out, buf, i);

	i++;
	assert(OUT_BUF_SIZE - i > 0);
	memmove(buf, buf + i, OUT_BUF_SIZE - i);
	bytes_read -= i;
	p = 0;

#define RD(xx) {							\
		unsigned char cc;					\
									\
		if (p < bytes_read)					\
			cc = buf[p++];					\
		else if ((hard_read(itrm->sock_in, &cc, 1)) <= 0)	\
			goto free_and_return;				\
		xx = cc;						\
	}

	RD(fg);

	if (!init_string(&path)) goto free_and_return;

	while (1) {
		RD(ch);
		if (!ch) break;
		add_char_to_string(&path, ch);
	}

	if (!init_string(&delete)) {
		done_string(&path);
		goto free_and_return;
	}

	while (1) {
		RD(ch);
		if (!ch) break;
		add_char_to_string(&delete, ch);
	}

#undef RD

	if (!*path.source) {
		dispatch_special(delete.source);

	} else {
		int blockh;
		unsigned char *param;
		int path_len, del_len, param_len;

		if (is_blocked() && fg) {
			if (*delete.source) unlink(delete.source);
			goto nasty_thing;
		}

		path_len = path.length;
		del_len = delete.length;
		param_len = path_len + del_len + 3;

		param = mem_alloc(param_len);
		if (!param) goto nasty_thing;

		param[0] = fg;
		memcpy(param + 1, path.source, path_len + 1);
		memcpy(param + 1 + path_len + 1, delete.source, del_len + 1);

		if (fg == 1) block_itrm(itrm->ctl_in);

		blockh = start_thread((void (*)(void *, int)) exec_thread,
				      param, param_len);
		if (blockh == -1) {
			if (fg == 1)
				unblock_itrm(itrm->ctl_in);
			mem_free(param);
			goto nasty_thing;
		}

		mem_free(param);

		if (fg == 1) {
			set_handlers(blockh, (void (*)(void *)) unblock_itrm_x,
				     NULL, (void (*)(void *)) unblock_itrm_x,
				     (void *) (long) blockh);
			/* block_itrm(itrm->ctl_in); */
		} else {
			set_handlers(blockh, close_handle, NULL, close_handle,
				     (void *) (long) blockh);
		}
	}

nasty_thing:
	done_string(&path);
	done_string(&delete);
	assert(OUT_BUF_SIZE - p > 0);
	memmove(buf, buf + p, OUT_BUF_SIZE - p);
	bytes_read -= p;

	goto qwerty;

free_and_return:
	free_trm(itrm);
}


static void
kbd_timeout(struct itrm *itrm)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, KBD_ESC, 0, 0);

	itrm->timer = -1;

	if (can_read(itrm->std_in)) {
		in_kbd(itrm);
		return;
	}

	assertm(itrm->qlen, "timeout on empty queue");
	if_assert_failed return;

	queue_event(itrm, (char *) &ev, sizeof(ev));

	if (--itrm->qlen)
		memmove(itrm->kqueue, itrm->kqueue + 1, itrm->qlen);

	while (process_queue(itrm));
}


/* Returns the length of the escape sequence */
static inline int
get_esc_code(unsigned char *str, int len, unsigned char *code, int *num)
{
	int pos;

	*num = 0;
	*code = '\0';

	for (pos = 2; pos < len; pos++) {
		if (!isdigit(str[pos]) || pos > 7) {
			*code = str[pos];

			return pos + 1;
		}
		*num = *num * 10 + str[pos] - '0';
	}

	return 0;
}

/* Define it to dump queue content in a readable form,
 * it may help to determine terminal sequences, and see what goes on. --Zas */
/* #define DEBUG_ITRM_QUEUE */

#ifdef DEBUG_ITRM_QUEUE
#include <ctype.h>	/* isprint() isspace() */
#endif

#ifdef CONFIG_MOUSE
static int
decode_mouse_position(struct itrm *itrm, int from)
{
	int position;

	position = (unsigned char) (itrm->kqueue[from]) - ' ' - 1
		 + ((int) ((unsigned char) (itrm->kqueue[from + 1]) - ' ' - 1) << 7);

	return (position & (1 << 13)) ? 0 : position;
}

#define get_mouse_x_position(itrm, esclen) decode_mouse_position(itrm, (esclen) + 1)
#define get_mouse_y_position(itrm, esclen) decode_mouse_position(itrm, (esclen) + 3)

/* Returns length of the escape sequence or -1 if the caller needs to set up
 * the ESC delay timer. */
static int
decode_terminal_mouse_escape_sequence(struct itrm *itrm, struct term_event *ev,
				      int el, int v)
{
	static int xterm_button = -1;
	struct term_event_mouse *mouse = &ev->info.mouse;

	if (itrm->qlen - el < 3)
		return -1;

	if (v == 5) {
		if (xterm_button == -1)
			xterm_button = 0;
		if (itrm->qlen - el < 5)
			return -1;

		mouse->x = get_mouse_x_position(itrm, el);
		mouse->y = get_mouse_y_position(itrm, el);

		switch ((itrm->kqueue[el] - ' ') ^ xterm_button) { /* Every event changes only one bit */
		    case TW_BUTT_LEFT:   mouse->button = B_LEFT | ( (xterm_button & TW_BUTT_LEFT) ? B_UP : B_DOWN ); break;
		    case TW_BUTT_MIDDLE: mouse->button = B_MIDDLE | ( (xterm_button & TW_BUTT_MIDDLE) ? B_UP : B_DOWN ); break;
		    case TW_BUTT_RIGHT:  mouse->button = B_RIGHT | ( (xterm_button & TW_BUTT_RIGHT) ? B_UP : B_DOWN ); break;
		    case 0: mouse->button = B_DRAG; break;
		    default: mouse->button = 0; /* Twin protocol error */
		}
		xterm_button = itrm->kqueue[el] - ' ';
		el += 5;
	} else {
		/* See terminal/mouse.h about details of the mouse reporting
		 * protocol and {struct term_event_mouse->button} bitmask
		 * structure. */
		mouse->x = itrm->kqueue[el+1] - ' ' - 1;
		mouse->y = itrm->kqueue[el+2] - ' ' - 1;

		/* There are rumours arising from remnants of code dating to
		 * the ancient Mikulas' times that bit 4 indicated B_DRAG.
		 * However, I didn't find on what terminal it should be ever
		 * supposed to work and it conflicts with wheels. So I removed
		 * the last remnants of the code as well. --pasky */

		mouse->button = (itrm->kqueue[el] & 7) | B_DOWN;
		/* smartglasses1 - rxvt wheel: */
		if (mouse->button == 3 && xterm_button != -1) {
			mouse->button = xterm_button | B_UP;
		}
		/* xterm wheel: */
		if ((itrm->kqueue[el] & 96) == 96) {
			mouse->button = (itrm->kqueue[el] & 1) ? B_WHEEL_DOWN : B_WHEEL_UP;
		}

		xterm_button = -1;
		/* XXX: Eterm/aterm uses rxvt-like reporting, but sends the
		 * release sequence for wheel. rxvt itself sends only press
		 * sequence. Since we can't reliably guess what we're talking
		 * with from $TERM, we will rather support Eterm/aterm, as in
		 * rxvt, at least each second wheel up move will work. */
		if (check_mouse_action(ev, B_DOWN))
#if 0
			    && !(getenv("TERM") && strstr("rxvt", getenv("TERM"))
				 && (ev->b & BM_BUTT) >= B_WHEEL_UP))
#endif
			xterm_button = get_mouse_button(ev);

		el += 3;
	}

	/* Postpone changing of the event type until all sanity
	 * checks have been done. */
	ev->ev = EVENT_MOUSE;

	return el;
}
#endif /* CONFIG_MOUSE */

/* Returns length of the escape sequence or -1 if the caller needs to set up
 * the ESC delay timer. */
static int
decode_terminal_escape_sequence(struct itrm *itrm, struct term_event *ev)
{
	unsigned char c;
	int key = -1, modifier = 0;
	int v;
	int el;

	if (itrm->qlen < 3) return -1;

	if (itrm->kqueue[2] == '[') {
		if (itrm->qlen >= 4
		    && itrm->kqueue[3] >= 'A'
		    && itrm->kqueue[3] <= 'L') {
			ev->info.keyboard.key = KBD_F1 + itrm->kqueue[3] - 'A';
			return 4;
		}

		return -1;
	}

	el = get_esc_code(itrm->kqueue, itrm->qlen, &c, &v);
#ifdef DEBUG_ITRM_QUEUE
	fprintf(stderr, "esc code: %c v=%d c=%c el=%d\n", itrm->kqueue[1], v, c, el);
	fflush(stderr);
#endif

	switch (c) {
	case 0: return -1;
	case 'A': key = KBD_UP; break;
	case 'B': key = KBD_DOWN; break;
	case 'C': key = KBD_RIGHT; break;
	case 'D': key = KBD_LEFT; break;
	case 'F':
	case 'e': key = KBD_END; break;
	case 'H': key = KBD_HOME; break;
	case 'I': key = KBD_PAGE_UP; break;
	case 'G': key = KBD_PAGE_DOWN; break;

	case 'z': switch (v) {
		case 247: key = KBD_INS; break;
		case 214: key = KBD_HOME; break;
		case 220: key = KBD_END; break;
		case 216: key = KBD_PAGE_UP; break;
		case 222: key = KBD_PAGE_DOWN; break;
		case 249: key = KBD_DEL; break;
		} break;

	case '~': switch (v) {
		case 1: key = KBD_HOME; break;
		case 2: key = KBD_INS; break;
		case 3: key = KBD_DEL; break;
		case 4: key = KBD_END; break;
		case 5: key = KBD_PAGE_UP; break;
		case 6: key = KBD_PAGE_DOWN; break;
		case 7: key = KBD_HOME; break;
		case 8: key = KBD_END; break;

		case 11: key = KBD_F1; break;
		case 12: key = KBD_F2; break;
		case 13: key = KBD_F3; break;
		case 14: key = KBD_F4; break;
		case 15: key = KBD_F5; break;

		case 17: key = KBD_F6; break;
		case 18: key = KBD_F7; break;
		case 19: key = KBD_F8; break;
		case 20: key = KBD_F9; break;
		case 21: key = KBD_F10; break;

		case 23: key = KBD_F11; break;
		case 24: key = KBD_F12; break;

		/* Give preference to F11 and F12 over shifted F1 and F2. */
		/*
		case 23: key = KBD_F1; modifier = KBD_SHIFT; break;
		case 24: key = KBD_F2; modifier = KBD_SHIFT; break;
		*/

		case 25: key = KBD_F3; modifier = KBD_SHIFT; break;
		case 26: key = KBD_F4; modifier = KBD_SHIFT; break;

		case 28: key = KBD_F5; modifier = KBD_SHIFT; break;
		case 29: key = KBD_F6; modifier = KBD_SHIFT; break;

		case 31: key = KBD_F7; modifier = KBD_SHIFT; break;
		case 32: key = KBD_F8; modifier = KBD_SHIFT; break;
		case 33: key = KBD_F9; modifier = KBD_SHIFT; break;
		case 34: key = KBD_F10; modifier = KBD_SHIFT; break;

		} break;

	case 'R': resize_terminal(); break;
	case 'M':
#ifdef CONFIG_MOUSE
		el = decode_terminal_mouse_escape_sequence(itrm, ev, el, v);
#endif /* CONFIG_MOUSE */
		break;
	}

	/* The event might have been changed to a mouse event */
	if (ev->ev == EVENT_KBD && key != -1) {
		ev->info.keyboard.key = key;
		ev->info.keyboard.modifier = modifier;
	}

	return el;
}

static void
set_kbd_event(struct term_event *ev, int key, int modifier)
{
	switch (key) {
	case ASCII_TAB:
		key = KBD_TAB;
		break;

	case ASCII_BS:
	case ASCII_DEL:
		key = KBD_BS;
		break;

	case ASCII_LF:
	case ASCII_CR:
		key = KBD_ENTER;
		break;

	default:
		if (key < ' ') {
			key += 'A' - 1;
			modifier = KBD_CTRL;
		}
	}

	ev->info.keyboard.key = key;
	ev->info.keyboard.modifier = modifier;
}

/* I feeeeeel the neeeed ... to rewrite this ... --pasky */
/* Just Do it ! --Zas */
static int
process_queue(struct itrm *itrm)
{
	struct term_event ev = INIT_TERM_EVENT(EVENT_KBD, -1, 0, 0);
	int el = 0;

	if (!itrm->qlen) goto end;

#ifdef DEBUG_ITRM_QUEUE
	{
		int i;

		/* Dump current queue in a readable form to stderr. */
		for (i = 0; i < itrm->qlen; i++)
			if (itrm->kqueue[i] == ASCII_ESC)
				fprintf(stderr, "ESC ");
			else if (isprint(itrm->kqueue[i]) && !isspace(itrm->kqueue[i]))
				fprintf(stderr, "%c ", itrm->kqueue[i]);
			else
				fprintf(stderr, "0x%02x ", itrm->kqueue[i]);

		fprintf(stderr, "\n");
		fflush(stderr);
	}
#endif /* DEBUG_ITRM_QUEUE */

	if (itrm->kqueue[0] == ASCII_ESC) {
		if (itrm->qlen < 2) goto ret;
		if (itrm->kqueue[1] == '[' || itrm->kqueue[1] == 'O') {
			el = decode_terminal_escape_sequence(itrm, &ev);

			if (el == -1) goto ret;

		} else {
			el = 2;

			if (itrm->kqueue[1] == ASCII_ESC) {
				if (itrm->qlen >= 3 &&
				    (itrm->kqueue[2] == '[' ||
				     itrm->kqueue[2] == 'O')) {
					el = 1;
				}

				set_kbd_event(&ev, KBD_ESC, 0);

			} else {
				set_kbd_event(&ev, itrm->kqueue[1], KBD_ALT);
			}
		}

	} else if (itrm->kqueue[0] == 0) {
		static const struct { int key, modifier; } os2xtd[256] = {
#include "terminal/key.inc"
		};

		if (itrm->qlen < 2) goto ret;
		ev.info.keyboard.key = os2xtd[itrm->kqueue[1]].key;

		if (!ev.info.keyboard.key)
			ev.info.keyboard.key = -1;

		ev.info.keyboard.modifier = os2xtd[itrm->kqueue[1]].modifier;
		el = 2;

	} else {
		el = 1;
		set_kbd_event(&ev, itrm->kqueue[0], 0);
	}

	assertm(itrm->qlen >= el, "event queue underflow");
	if_assert_failed { itrm->qlen = el; }

	itrm->qlen -= el;

	/* The call to decode_terminal_escape_sequence() might have changed the
	 * keyboard event to a mouse event. */
	if (ev.ev == EVENT_MOUSE || ev.info.keyboard.key != -1)
		queue_event(itrm, (char *) &ev, sizeof(ev));

	if (itrm->qlen)
		memmove(itrm->kqueue, itrm->kqueue + el, itrm->qlen);

end:
	if (itrm->qlen < IN_BUF_SIZE)
		set_handlers(itrm->std_in, (void (*)(void *)) in_kbd, NULL,
			     (void (*)(void *)) free_trm, itrm);
	return el;

ret:
	itrm->timer = install_timer(ESC_TIMEOUT, (void (*)(void *)) kbd_timeout,
				    itrm);

	return 0;
}


static void
in_kbd(struct itrm *itrm)
{
	int r;

	if (!can_read(itrm->std_in)) return;

	if (itrm->timer != -1) {
		kill_timer(itrm->timer);
		itrm->timer = -1;
	}

	if (itrm->qlen >= IN_BUF_SIZE) {
		set_handlers(itrm->std_in, NULL, NULL,
			     (void (*)(void *)) free_trm, itrm);
		while (process_queue(itrm));
		return;
	}

	r = safe_read(itrm->std_in, itrm->kqueue + itrm->qlen,
		      IN_BUF_SIZE - itrm->qlen);
	if (r <= 0) {
		free_trm(itrm);
		return;
	}

	itrm->qlen += r;
	if (itrm->qlen > IN_BUF_SIZE) {
		ERROR(gettext("Too many bytes read from the itrm!"));
		itrm->qlen = IN_BUF_SIZE;
	}

	while (process_queue(itrm));
}
