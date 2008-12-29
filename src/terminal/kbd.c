/** Support for keyboard interface
 * @file
 *
 * @todo TODO: move stuff from here to itrm.{c,h} and mouse.{c,h}
 */

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
#include "main/select.h"
#include "main/timer.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "terminal/hardio.h"
#include "terminal/itrm.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"

struct itrm *ditrm = NULL;

static void free_itrm(struct itrm *);
static void in_kbd(struct itrm *);
static void in_sock(struct itrm *);
static int process_queue(struct itrm *);
static void handle_itrm_stdin(struct itrm *);
static void unhandle_itrm_stdin(struct itrm *);

#ifdef CONFIG_DEBUG
/** This hack makes GCC put enum term_event_special_key in the debug
 * information even though it is not otherwise used.  The const
 * prevents an unused-variable warning.  */
static const enum term_event_special_key dummy_term_event_special_key;
#endif


int
is_blocked(void)
{
	return ditrm && ditrm->blocked;
}


void
free_all_itrms(void)
{
	if (ditrm) free_itrm(ditrm);
}


/** A select_handler_T write_func for itrm_out.sock.  This is called
 * when there is data in @c itrm->out.queue and it is possible to write
 * it to @c itrm->out.sock.  When @c itrm->out.queue becomes empty, this
 * handler is temporarily removed.  */
static void
itrm_queue_write(struct itrm *itrm)
{
	int written;
	int qlen = int_min(itrm->out.queue.len, 128);

	assertm(qlen, "event queue empty");
	if_assert_failed return;

	written = safe_write(itrm->out.sock, itrm->out.queue.data, qlen);
	if (written <= 0) {
		if (written < 0) free_itrm(itrm); /* write error */
		return;
	}

	itrm->out.queue.len -= written;

	if (itrm->out.queue.len == 0) {
		set_handlers(itrm->out.sock,
			     get_handler(itrm->out.sock, SELECT_HANDLER_READ),
			     NULL,
			     get_handler(itrm->out.sock, SELECT_HANDLER_ERROR),
			     get_handler(itrm->out.sock, SELECT_HANDLER_DATA));
	} else {
		assert(itrm->out.queue.len > 0);
		memmove(itrm->out.queue.data, itrm->out.queue.data + written, itrm->out.queue.len);
	}
}


void
itrm_queue_event(struct itrm *itrm, unsigned char *data, int len)
{
	int w = 0;

	if (!len) return;

	if (!itrm->out.queue.len && can_write(itrm->out.sock)) {
		w = safe_write(itrm->out.sock, data, len);
		if (w <= 0 && HPUX_PIPE) {
			register_bottom_half(free_itrm, itrm);
			return;
		}
	}

	if (w < len) {
		int left = len - w;
		unsigned char *c = mem_realloc(itrm->out.queue.data,
					       itrm->out.queue.len + left);

		if (!c) {
			free_itrm(itrm);
			return;
		}

		itrm->out.queue.data = c;
		memcpy(itrm->out.queue.data + itrm->out.queue.len, data + w, left);
		itrm->out.queue.len += left;
		set_handlers(itrm->out.sock,
			     get_handler(itrm->out.sock, SELECT_HANDLER_READ),
			     (select_handler_T) itrm_queue_write,
			     (select_handler_T) free_itrm, itrm);
	}
}


void
kbd_ctrl_c(void)
{
	struct interlink_event ev;

	if (!ditrm) return;
	set_kbd_interlink_event(&ev, KBD_CTRL_C, KBD_MOD_NONE);
	itrm_queue_event(ditrm, (unsigned char *) &ev, sizeof(ev));
}

#define write_sequence(fd, seq) \
	hard_write(fd, seq, sizeof(seq) - 1)


#define INIT_TERMINAL_SEQ	"\033)0\0337"	/**< Special Character and Line Drawing Set, Save Cursor */
#define INIT_ALT_SCREEN_SEQ	"\033[?47h"	/**< Use Alternate Screen Buffer */

static void
send_init_sequence(int h, int altscreen)
{
	write_sequence(h, INIT_TERMINAL_SEQ);

	/* If alternate screen is supported switch to it. */
	if (altscreen) {
		write_sequence(h, INIT_ALT_SCREEN_SEQ);
	}
#ifdef CONFIG_MOUSE
	send_mouse_init_sequence(h);
#endif
}

#define DONE_CLS_SEQ		"\033[2J"	/**< Erase in Display, Clear All */
#define DONE_TERMINAL_SEQ	"\0338\r \b"	/**< Restore Cursor (DECRC) + ??? */
#define DONE_ALT_SCREEN_SEQ	"\033[?47l"	/**< Use Normal Screen Buffer */

static void
send_done_sequence(int h, int altscreen)
{
	write_sequence(h, DONE_CLS_SEQ);

#ifdef CONFIG_MOUSE
	send_mouse_done_sequence(h);
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
	struct interlink_event ev;
	int width, height;

	get_terminal_size(ditrm->out.std, &width, &height);
	set_resize_interlink_event(&ev, width, height);
	itrm_queue_event(ditrm, (char *) &ev, sizeof(ev));
}

void
get_terminal_name(unsigned char name[MAX_TERM_LEN])
{
	unsigned char *term = getenv("TERM");
	int i;

	memset(name, 0, MAX_TERM_LEN);

	if (!term) return;

	for (i = 0; term[i] != 0 && i < MAX_TERM_LEN - 1; i++)
		name[i] = isident(term[i]) ? term[i] : '-';
}


static int
setraw(struct itrm *itrm, int save_orig)
{
	struct termios t;
	long vdisable;

	memset(&t, 0, sizeof(t));
	if (tcgetattr(itrm->in.ctl, &t)) return -1;

	if (save_orig) copy_struct(&itrm->t, &t);

#ifdef _POSIX_VDISABLE
	vdisable = _POSIX_VDISABLE;
#else
	vdisable = fpathconf(itrm->in.ctl, _PC_VDISABLE);
#endif
	if (vdisable != -1 && t.c_cc[VERASE] == vdisable)
		itrm->verase = -1;
	else
		itrm->verase = (unsigned char) t.c_cc[VERASE];

	elinks_cfmakeraw(&t);
	t.c_lflag |= ISIG;
#ifdef TOSTOP
	t.c_lflag |= TOSTOP;
#endif
	t.c_oflag |= OPOST;
	if (tcsetattr(itrm->in.ctl, TCSANOW, &t)) return -1;

	return 0;
}

/** Construct the struct itrm of this process, make ::ditrm point to it,
 * set up select() handlers, and send the initial interlink packet.
 *
 * The first five parameters are file descriptors that this function
 * saves in submembers of struct itrm, and for which this function may
 * set select() handlers.  Please see the definitions of struct
 * itrm_in and struct itrm_out for further explanations.
 *
 * @param std_in	itrm_in.std: read tty device (or pipe)
 * @param std_out	itrm_out.std: write tty device (or pipe)
 * @param sock_in	itrm_in.sock
 *			- If master: == @a std_out (masterhood flag)
 *			- If slave: read socket from master
 * @param sock_out	itrm_out.sock
 *			- If master: write pipe to same process
 *			- If slave: write socket to master
 * @param ctl_in	itrm_in.ctl: control tty device
 *
 * The remaining three parameters control the initial interlink packet.
 *
 * @param init_string	A string to be passed to the master process.  Need
 *			not be null-terminated.  If @a remote == 0, this is
 *			a URI.  Otherwise, this is a remote command.
 * @param init_len	The length of init_string, in bytes.
 * @param remote	= 0 if asking the master to start a new session
 *			and display it via this process.  Otherwise,
 *			enum ::remote_session_flags.  */
void
handle_trm(int std_in, int std_out, int sock_in, int sock_out, int ctl_in,
	   void *init_string, int init_len, int remote)
{
	struct itrm *itrm;
	struct terminal_info info;
	struct interlink_event_size *size = &info.event.info.size;
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

	itrm->in.queue.data = mem_calloc(1, ITRM_IN_QUEUE_SIZE);
	if (!itrm->in.queue.data) {
		mem_free(itrm);
		return;
	}

	ditrm = itrm;
	itrm->in.std = std_in;
	itrm->out.std = std_out;
	itrm->in.sock = sock_in;
	itrm->out.sock = sock_out;
	itrm->in.ctl = ctl_in;
	itrm->timer = TIMER_ID_UNDEF;
	itrm->remote = !!remote;

	/* If the master does not tell which charset it's using in
	 * this terminal, assume it's some ISO 8859.  Because that's
	 * what older versions of ELinks did.  */
	itrm->title_codepage = get_cp_index("ISO-8859-1");

	/* FIXME: Combination altscreen + xwin does not work as it should,
	 * mouse clicks are reportedly partially ignored. */
	if (info.system_env & (ENV_SCREEN | ENV_XWIN))
		itrm->altscreen = 1;

	if (!remote) {
		if (ctl_in >= 0) setraw(itrm, 1);
		send_init_sequence(std_out, itrm->altscreen);
		handle_terminal_resize(ctl_in, resize_terminal);
#ifdef CONFIG_MOUSE
		enable_mouse();
#endif
		handle_itrm_stdin(itrm);
	} else {
		/* elinks -remote may not have a valid stdin if not run from a tty (bug 938) */
		if (std_in >= 0) handle_itrm_stdin(itrm);
	}

	if (sock_in != std_out)
		set_handlers(sock_in, (select_handler_T) in_sock,
			     NULL, (select_handler_T) free_itrm, itrm);

	get_terminal_name(info.name);

	ts = get_cwd();
	if (ts) {
		memcpy(info.cwd, ts, int_min(strlen(ts), MAX_CWD_LEN));
		mem_free(ts);
	}

	itrm_queue_event(itrm, (char *) &info, TERMINAL_INFO_SIZE);
	itrm_queue_event(itrm, (char *) init_string, init_len);
}


/** A select_handler_T read_func and error_func for the pipe (long) @a h.
 * This is called when the subprocess started on the terminal of this
 * ELinks process exits.  ELinks then resumes using the terminal.  */
static void
unblock_itrm_x(void *h)
{
	close_handle(h);
	if (!ditrm) return;
	unblock_itrm();
	resize_terminal();
}


int
unblock_itrm(void)
{
	if (!ditrm) return -1;

	if (ditrm->in.ctl >= 0 && setraw(ditrm, 0)) return -1;
	ditrm->blocked = 0;
	send_init_sequence(ditrm->out.std, ditrm->altscreen);

	handle_itrm_stdin(ditrm);
	resume_mouse(ditrm->mouse_h);

	handle_terminal_resize(ditrm->in.ctl, resize_terminal);
	unblock_stdin();

	return 0;
}


void
block_itrm(void)
{
	if (!ditrm) return;

	ditrm->blocked = 1;
	block_stdin();
	kill_timer(&ditrm->timer);
	ditrm->in.queue.len = 0;
	unhandle_terminal_resize(ditrm->in.ctl);
	send_done_sequence(ditrm->out.std, ditrm->altscreen);
	tcsetattr(ditrm->in.ctl, TCSANOW, &ditrm->t);
	unhandle_itrm_stdin(ditrm);
	suspend_mouse(ditrm->mouse_h);
}


static void
free_itrm(struct itrm *itrm)
{
	if (!itrm) return;

	if (!itrm->remote) {
		if (itrm->orig_title && *itrm->orig_title) {
			set_window_title(itrm->orig_title, itrm->title_codepage);

		} else if (itrm->touched_title) {
			/* Set the window title to the value of $TERM if X11
			 * wasn't compiled in. Should hopefully make at least
			 * half the users happy. (debian bug #312955) */
			unsigned char title[MAX_TERM_LEN];

			get_terminal_name(title);
			if (*title)
				set_window_title(title,
						 get_cp_index("US-ASCII"));
		}


		unhandle_terminal_resize(itrm->in.ctl);
#ifdef CONFIG_MOUSE
		disable_mouse();
#endif
		send_done_sequence(itrm->out.std, itrm->altscreen);
		tcsetattr(itrm->in.ctl, TCSANOW, &itrm->t);
	}

	mem_free_set(&itrm->orig_title, NULL);

	/* elinks -remote may not have a valid stdin if not run from a tty (bug 938) */
	if (!itrm->remote || itrm->in.std >= 0) clear_handlers(itrm->in.std);
	clear_handlers(itrm->in.sock);
	clear_handlers(itrm->out.std);
	clear_handlers(itrm->out.sock);

	kill_timer(&itrm->timer);

	if (itrm == ditrm) ditrm = NULL;
	mem_free_if(itrm->out.queue.data);
	mem_free_if(itrm->in.queue.data);
	mem_free(itrm);
}

/** Resize terminal to dimensions specified by @a text string.
 * @a text should look like "width,height,old-width,old-height"
 * where width and height are integers. */
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
				if (ditrm->remote)
					break;

				if (!ditrm->orig_title)
					ditrm->orig_title = get_window_title();
				ditrm->touched_title = 1;
			}
			/* TODO: Is it really possible to get here with
			 * ditrm == NULL, and which charset would then
			 * be most appropriate?  */
			set_window_title(text + 1,
					 ditrm ? ditrm->title_codepage
					 : get_cp_index("US-ASCII"));
			break;
		case TERM_FN_RESIZE:
			if (ditrm && ditrm->remote)
				break;

			resize_terminal_from_str(text + 1);
			break;
		case TERM_FN_TITLE_CODEPAGE:
			if (ditrm) {
				int cp = get_cp_index(text + 1);

				/* If the master sends the name of an
				 * unrecognized charset, assume only
				 * that it's ASCII compatible.  */
				if (cp == -1)
					cp = get_cp_index("US-ASCII");
				ditrm->title_codepage = cp;
			}
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

/** A select_handler_T read_func for itrm_in.sock.  A slave process
 * calls this when the master sends it data to be displayed.  The
 * master process never calls this.  */
static void
in_sock(struct itrm *itrm)
{
	struct string path;
	struct string delete;
	char ch;
	int fg; /* enum term_exec */
	ssize_t bytes_read, i, p;
	unsigned char buf[ITRM_OUT_QUEUE_SIZE];

	bytes_read = safe_read(itrm->in.sock, buf, ITRM_OUT_QUEUE_SIZE);
	if (bytes_read <= 0) goto free_and_return;

qwerty:
	for (i = 0; i < bytes_read; i++)
		if (!buf[i])
			goto has_nul_byte;

	safe_hard_write(itrm->out.std, buf, bytes_read);
	return;

has_nul_byte:
	if (i) safe_hard_write(itrm->out.std, buf, i);

	i++;
	assert(ITRM_OUT_QUEUE_SIZE - i > 0);
	memmove(buf, buf + i, ITRM_OUT_QUEUE_SIZE - i);
	bytes_read -= i;
	p = 0;

#define RD(xx) {							\
		unsigned char cc;					\
									\
		if (p < bytes_read)					\
			cc = buf[p++];					\
		else if ((hard_read(itrm->in.sock, &cc, 1)) <= 0)	\
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

		/* TODO: Should this be changed to allow TERM_EXEC_NEWWIN
		 * in a blocked terminal?  There is similar code in
		 * exec_on_terminal().  --KON, 2007 */
		if (is_blocked() && fg != TERM_EXEC_BG) {
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

		if (fg == TERM_EXEC_FG) block_itrm();

		blockh = start_thread((void (*)(void *, int)) exec_thread,
				      param, param_len);
		mem_free(param);

		if (blockh == -1) {
			if (fg == TERM_EXEC_FG)
				unblock_itrm();

			goto nasty_thing;
		}

		if (fg == TERM_EXEC_FG) {
			set_handlers(blockh, (select_handler_T) unblock_itrm_x,
				     NULL, (select_handler_T) unblock_itrm_x,
				     (void *) (long) blockh);

		} else {
			set_handlers(blockh, close_handle, NULL, close_handle,
				     (void *) (long) blockh);
		}
	}

nasty_thing:
	done_string(&path);
	done_string(&delete);
	assert(ITRM_OUT_QUEUE_SIZE - p > 0);
	memmove(buf, buf + p, ITRM_OUT_QUEUE_SIZE - p);
	bytes_read -= p;

	goto qwerty;

free_and_return:
	free_itrm(itrm);
}


/** Parse an ECMA-48 control sequence that was received from a
 * terminal.  Extract the Final Byte (if there are no Intermediate
 * Bytes) and the value of the first parameter (if it is an integer).
 *
 * This function assumes the control sequence begins with a CSI -
 * CONTROL SEQUENCE INTRODUCER encoded as ESC [.  (ECMA-48 also allows
 * 0x9B as a single-byte CSI, but we don't support that here.)
 *
 * @returns one of:
 * - -1 if the control sequence is not yet complete; the caller sets a timer.
 * - 0 if the control sequence does not comply with ECMA-48.
 * - The length of the control sequence otherwise.  */
static inline int
get_esc_code(unsigned char *str, int len, unsigned char *final_byte,
	     int *first_param_value)
{
	const int parameter_pos = 2;
	int intermediate_pos;
	int final_pos;
	int pos;

	*final_byte = '\0';
	*first_param_value = 0;

	/* Parameter Bytes */
	pos = parameter_pos;
	while (pos < len && str[pos] >= 0x30 && str[pos] <= 0x3F)
		++pos;

	/* Intermediate Bytes */
	intermediate_pos = pos;
	while (pos < len && str[pos] >= 0x20 && str[pos] <= 0x2F)
		++pos;

	/* Final Byte */
	final_pos = pos;
	if (pos >= len)
		return -1;
	if (!(str[pos] >= 0x40 && str[pos] <= 0x7E))
		return 0;

	/* The control sequence seems OK.  If the first Parameter
	 * Byte indicates that the parameter string is formatted
	 * as specified in clause 5.4.2 of ECMA-48, and the first
	 * parameter is an integer, then compute its value.
	 * (We need not check @len here because the loop cannot get
	 * past the Final Byte.)  */
	for (pos = parameter_pos; str[pos] >= 0x30 && str[pos] <= 0x39; ++pos)
		*first_param_value = *first_param_value * 10 + str[pos] - 0x30;
	/* If the first parameter contains an embedded separator, then
	 * the value is not an integer, so discard what we computed.  */
	if (str[pos] == 0x3A)
		*first_param_value = 0;

	/* The meaning of the Final Byte depends on the Intermediate
	 * Bytes.  Because we don't currently need to recognize any
	 * control sequences that use Intermediate Bytes, we just
	 * discard the Final Byte if there are any Intermediate
	 * Bytes.  */
	if (intermediate_pos == final_pos)
		*final_byte = str[final_pos];

	return final_pos + 1;
}

/* Define it to dump queue content in a readable form,
 * it may help to determine terminal sequences, and see what goes on. --Zas */
/* #define DEBUG_ITRM_QUEUE */

#ifdef DEBUG_ITRM_QUEUE
#include <stdio.h>
#include <ctype.h>	/* isprint() isspace() */
#endif

/** Decode a control sequence that begins with CSI (CONTROL SEQUENCE
 * INTRODUCER) encoded as ESC [, and set @a *ev accordingly.
 * (ECMA-48 also allows 0x9B as a single-byte CSI, but we don't
 * support that here.)
 *
 * @returns one of:
 * - -1 if the control sequence is not yet complete; the caller sets a timer.
 * - 0 if the control sequence should be parsed by some other function.
 * - The length of the control sequence otherwise.
 * Returning >0 does not imply this function has altered @a *ev.  */
static int
decode_terminal_escape_sequence(struct itrm *itrm, struct interlink_event *ev)
{
	struct term_event_keyboard kbd = { KBD_UNDEF, KBD_MOD_NONE };
	unsigned char c;
	int v;
	int el;

	if (itrm->in.queue.len < 3) return -1;

	if (itrm->in.queue.data[2] == '[') {
		/* The terminfo entry for linux has "kf1=\E[[A", etc.
		 * These are not control sequences compliant with
		 * clause 5.4 of ECMA-48.  (According to ECMA-48,
		 * "\E[[" is SRS - START REVERSED STRING.)  */
		if (itrm->in.queue.len >= 4
		    && itrm->in.queue.data[3] >= 'A'
		    && itrm->in.queue.data[3] <= 'L') {
			kbd.key = number_to_kbd_fkey(itrm->in.queue.data[3] - 'A' + 1);
			set_kbd_interlink_event(ev, kbd.key, kbd.modifier);
			return 4;
		}

		return -1;
	}

	el = get_esc_code(itrm->in.queue.data, itrm->in.queue.len, &c, &v);
	if (el == -1) {
		/* If the control sequence is incomplete but itrm->in.queue
		 * is already full, then we must not wait for more input:
		 * kbd_timeout might call in_kbd and thus process_input
		 * and come right back here.  Better just reject the whole
		 * thing and let the initial CSI be handled as Alt-[.  */
		if (itrm->in.queue.len == ITRM_IN_QUEUE_SIZE)
			return 0;
		else
			return -1;
	}
#ifdef DEBUG_ITRM_QUEUE
	fprintf(stderr, "esc code: %c v=%d c=%c el=%d\n", itrm->in.queue.data[1], v, c, el);
	fflush(stderr);
#endif

	/* The following information should be listed for each escape
	 * sequence recognized here:
	 *
	 * 1. Which control function ECMA-48 assigns to the sequence.
	 *    Put parentheses around this if the control function
	 *    seems unrelated to how ELinks actually treats the
	 *    sequence.  Write "private" if it is a control sequence
	 *    reserved for private or experimental use in ECMA-48.
	 *    (Those have a Final Byte in the range 0x70 to 0x7F,
	 *    optionally preceded by a single Intermediate Byte 0x20.)
	 *
	 * 2. The capname used by Terminfo, if any.  These should help
	 *    when ELinks is eventually changed to read escape
	 *    sequences from Terminfo (bug 96).
	 *
	 * 3. The $TERM identifier of some terminal that generates
	 *    this escape sequence with the meaning expected by
	 *    ELinks.  Escape sequences with no known terminal may end
	 *    up being removed from ELinks when bug 96 is fixed.
	 */					/* ECMA-48 Terminfo $TERM */
	switch (c) {				/* ------- -------- ----- */
	case 'A': kbd.key = KBD_UP; break;	/*    CUU  kcuu1    vt200 */
	case 'B': kbd.key = KBD_DOWN; break;	/*    CUD  kcud1    vt200 */
	case 'C': kbd.key = KBD_RIGHT; break;	/*    CUF  kcuf1    vt200 */
	case 'D': kbd.key = KBD_LEFT; break;	/*    CUB  kcub1    vt200 */
	case 'F':				/*   (CPL) kend     cons25 */
	case 'e': kbd.key = KBD_END; break;	/*   (VPR) kend */
	case 'H': kbd.key = KBD_HOME; break;	/*    CUP  khome    cons25 */
	case 'I': kbd.key = KBD_PAGE_UP; break; /*   (CHT) kpp      cons25 */
	case 'G': kbd.key = KBD_PAGE_DOWN; break; /* (CHA) knp      cons25 */
	case 'L': kbd.key = KBD_INS; break;	/*   (IL)  kich1    cons25 */
/* Free BSD (TERM=cons25 etc.) */
/*	case 'M': kbd.key = KBD_F1; break;*/	/*   (DL)  kf1      cons25 */
	case 'N': kbd.key = KBD_F2; break;	/*   (EF)  kf2      cons25 */
	case 'O': kbd.key = KBD_F3; break;	/*   (EA)  kf3      cons25 */
	case 'P': kbd.key = KBD_F4; break;	/*   (DCH) kf4      cons25 */
	case 'Q': kbd.key = KBD_F5; break;	/*   (SEE) kf5      cons25 */
/*	case 'R': kbd.key = KBD_F6; break;*/	/*   (CPR) kf6      cons25 */
	case 'S': kbd.key = KBD_F7; break;	/*   (SU)  kf7      cons25 */
	case 'T': kbd.key = KBD_F8; break;	/*   (SD)  kf8      cons25 */
	case 'U': kbd.key = KBD_F9; break;	/*   (NP)  kf9      cons25 */
	case 'V': kbd.key = KBD_F10; break;	/*   (PP)  kf10     cons25 */
	case 'W': kbd.key = KBD_F11; break;	/*   (CTC) kf11     cons25 */
	case 'X': kbd.key = KBD_F12; break;	/*   (ECH) kf12     cons25 */

	case 'Z':				/*    CBT  kcbt     cons25 */
		kbd.key = KBD_TAB; kbd.modifier = KBD_MOD_SHIFT; break;

	case 'z': switch (v) {			/* private */
		case 247: kbd.key = KBD_INS; break;     /* kich1 */
		case 214: kbd.key = KBD_HOME; break;	/* khome    sun */
		case 220: kbd.key = KBD_END; break;	/* kend     sun */
		case 216: kbd.key = KBD_PAGE_UP; break; /* kpp      sun */
		case 222: kbd.key = KBD_PAGE_DOWN; break; /* knp    sun */
		case 249: kbd.key = KBD_DEL; break;	/* kdch1 */
		} break;

	case '~': switch (v) {			/* private */
		case 1: kbd.key = KBD_HOME; break;      /* khome    linux */
		case 2: kbd.key = KBD_INS; break;	/* kich1    linux */
		case 3: kbd.key = KBD_DEL; break;	/* kdch1    linux */
		case 4: kbd.key = KBD_END; break;	/* kend     linux */
		case 5: kbd.key = KBD_PAGE_UP; break;	/* kpp      linux */
		case 6: kbd.key = KBD_PAGE_DOWN; break; /* knp      linux */
		case 7: kbd.key = KBD_HOME; break;	/* khome    rxvt */
		case 8: kbd.key = KBD_END; break;	/* kend     rxvt */

		case 11: kbd.key = KBD_F1; break;	/* kf1      rxvt */
		case 12: kbd.key = KBD_F2; break;	/* kf2      rxvt */
		case 13: kbd.key = KBD_F3; break;	/* kf3      rxvt */
		case 14: kbd.key = KBD_F4; break;	/* kf4      rxvt */
		case 15: kbd.key = KBD_F5; break;	/* kf5      rxvt */

		case 17: kbd.key = KBD_F6; break;	/* kf6      vt200 */
		case 18: kbd.key = KBD_F7; break;	/* kf7      vt200 */
		case 19: kbd.key = KBD_F8; break;	/* kf8      vt200 */
		case 20: kbd.key = KBD_F9; break;	/* kf9      vt200 */
		case 21: kbd.key = KBD_F10; break;	/* kf10     vt200 */

		case 23: kbd.key = KBD_F11; break;	/* kf11     vt200 */
		case 24: kbd.key = KBD_F12; break;	/* kf12     vt200 */

		/* Give preference to F11 and F12 over shifted F1 and F2. */
		/*
		case 23: kbd.key = KBD_F1; kbd.modifier = KBD_MOD_SHIFT; break;
		case 24: kbd.key = KBD_F2; kbd.modifier = KBD_MOD_SHIFT; break;
		*/

		case 25: kbd.key = KBD_F3; kbd.modifier = KBD_MOD_SHIFT; break;
		case 26: kbd.key = KBD_F4; kbd.modifier = KBD_MOD_SHIFT; break;

		case 28: kbd.key = KBD_F5; kbd.modifier = KBD_MOD_SHIFT; break;
		case 29: kbd.key = KBD_F6; kbd.modifier = KBD_MOD_SHIFT; break;

		case 31: kbd.key = KBD_F7; kbd.modifier = KBD_MOD_SHIFT; break;
		case 32: kbd.key = KBD_F8; kbd.modifier = KBD_MOD_SHIFT; break;
		case 33: kbd.key = KBD_F9; kbd.modifier = KBD_MOD_SHIFT; break;
		case 34: kbd.key = KBD_F10; kbd.modifier = KBD_MOD_SHIFT; break;

		} break;

	case 'R': resize_terminal(); break;	/*    CPR  u6 */
	case 'M':                               /*   (DL)  kmous    xterm */
#ifdef CONFIG_MOUSE
		el = decode_terminal_mouse_escape_sequence(itrm, ev, el, v);
#endif /* CONFIG_MOUSE */
		break;
	}

	/* KBD_UNDEF here means it was unrecognized or a mouse event.  */
	if (kbd.key != KBD_UNDEF)
		set_kbd_interlink_event(ev, kbd.key, kbd.modifier);

	return el;
}

/** Decode an escape sequence that begins with SS3 (SINGLE SHIFT 3).
 * These are used for application cursor keys and the application keypad.
 * @returns one of:
 * - -1 if the escape sequence is not yet complete; the caller sets a timer.
 * - 0 if the escape sequence should be parsed by some other function.
 * - The length of the escape sequence otherwise.
 * Returning >0 does not imply this function has altered @a *ev.  */
static int
decode_terminal_application_key(struct itrm *itrm, struct interlink_event *ev)
{
	unsigned char c;
	struct interlink_event_keyboard kbd = { KBD_UNDEF, KBD_MOD_NONE };

	assert(itrm->in.queue.len >= 2);
	assert(itrm->in.queue.data[0] == ASCII_ESC);
	assert(itrm->in.queue.data[1] == 0x4F); /* == 'O', incidentally */
	if_assert_failed return 0;

	if (itrm->in.queue.len < 3) return -1;
	/* According to ECMA-35 section 8.4, a single (possibly multibyte)
	 * character follows the SS3.  We now assume the code identifies
	 * GL as the single-shift area and the designated set has 94
	 * characters.  */
	c = itrm->in.queue.data[2];
	if (c < 0x21 || c > 0x7E) return 0;

	switch (c) {					/* Terminfo $TERM */
	case ' ': kbd.key = ' '; break;			/*          xterm */
	case 'A': kbd.key = KBD_UP; break;		/* kcuu1    vt100 */
	case 'B': kbd.key = KBD_DOWN; break;		/* kcud1    vt100 */
	case 'C': kbd.key = KBD_RIGHT; break;		/* kcuf1    vt100 */
	case 'D': kbd.key = KBD_LEFT; break;		/* kcub1    vt100 */
	case 'F': kbd.key = KBD_END; break;		/* kend     xterm */
	case 'H': kbd.key = KBD_HOME; break;		/* khome    xterm */
	case 'I': kbd.key = KBD_TAB; break;		/*          xterm */
	case 'M': kbd.key = KBD_ENTER; break;		/* kent     vt100 */
		/* FIXME: xterm generates ESC O 2 P for Shift-PF1 */
	case 'P': kbd.key = KBD_F1; break;		/* kf1      vt100 */
	case 'Q': kbd.key = KBD_F2; break;		/* kf2      vt100 */
	case 'R': kbd.key = KBD_F3; break;		/* kf3      vt100 */
	case 'S': kbd.key = KBD_F4; break;		/* kf4      vt100 */
	case 'X': kbd.key = '='; break;			/*          xterm */

	case 'j': case 'k': case 'l': case 'm': /* *+,-             xterm */
	case 'n': case 'o': case 'p': case 'q': /* ./01             xterm */
	case 'r': case 's': case 't': case 'u': /* 2345             xterm */
	case 'v': case 'w': case 'x': case 'y': /* 6789             xterm */
		kbd.key = c - 'p' + '0'; break;
	}
	if (kbd.key != KBD_UNDEF)
		copy_struct(&ev->info.keyboard, &kbd);

	return 3;		/* even if we didn't recognize it */
}


/** Initialize @a *ev to match the byte @a key received from the terminal.
 * @a key must not be a value from enum term_event_special_key.  */
static void
set_kbd_event(const struct itrm *itrm, struct interlink_event *ev,
	      int key, term_event_modifier_T modifier)
{
	if (key == itrm->verase)
		key = KBD_BS;
	else switch (key) {
	case ASCII_TAB:
		key = KBD_TAB;
		break;
	case ASCII_DEL:	/* often overridden by itrm->verase above */
		key = KBD_DEL;
		break;
	case ASCII_LF:
	case ASCII_CR:
		key = KBD_ENTER;
		break;

	case ASCII_ESC:
		key = KBD_ESC;
		break;

	case ASCII_BS:	/* often overridden by itrm->verase above */
	default:
		if (key < ' ') {
			key += 'A' - 1;
			modifier |= KBD_MOD_CTRL;
		}
	}

	set_kbd_interlink_event(ev, key, modifier);
}

/** Timer callback for itrm.timer.  As explained in install_timer(),
 * this function must erase the expired timer ID from all variables.  */
static void
kbd_timeout(struct itrm *itrm)
{
	struct interlink_event ev;
	int el;

	itrm->timer = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */

	assertm(itrm->in.queue.len, "timeout on empty queue");
	assert(!itrm->blocked);	/* block_itrm should have killed itrm->timer */
	if_assert_failed return;

	if (can_read(itrm->in.std)) {
		in_kbd(itrm);
		return;
	}

	if (itrm->in.queue.len >= 2 && itrm->in.queue.data[0] == ASCII_ESC) {
		/* This is used for ESC [ and ESC O.  */
		set_kbd_event(itrm, &ev, itrm->in.queue.data[1], KBD_MOD_ALT);
		el = 2;
	} else {
		set_kbd_event(itrm, &ev, itrm->in.queue.data[0], KBD_MOD_NONE);
		el = 1;
	}
	itrm_queue_event(itrm, (char *) &ev, sizeof(ev));

	itrm->in.queue.len -= el;
	if (itrm->in.queue.len)
		memmove(itrm->in.queue.data, itrm->in.queue.data + el, itrm->in.queue.len);

	while (process_queue(itrm));
}

/** Parse one event from itrm_in.queue and append to itrm_out.queue.
 * @pre On entry, @a *itrm must not be blocked.
 * @returns the number of bytes removed from itrm->in.queue; at least 0.
 * @post If this function leaves the queue not full, it also reenables
 * reading from itrm->in.std.  (Because it does not add to the queue,
 * it never need disable reading.)  */
static int
process_queue(struct itrm *itrm)
{
	struct interlink_event ev;
	int el = 0;

	if (!itrm->in.queue.len) goto return_without_event;
	assert(!itrm->blocked);
	if_assert_failed return 0; /* unlike goto, don't enable reading */

	set_kbd_interlink_event(&ev, KBD_UNDEF, KBD_MOD_NONE);

#ifdef DEBUG_ITRM_QUEUE
	{
		int i;

		/* Dump current queue in a readable form to stderr. */
		for (i = 0; i < itrm->in.queue.len; i++)
			if (itrm->in.queue.data[i] == ASCII_ESC)
				fprintf(stderr, "ESC ");
			else if (isprint(itrm->in.queue.data[i]) && !isspace(itrm->in.queue.data[i]))
				fprintf(stderr, "%c ", itrm->in.queue.data[i]);
			else
				fprintf(stderr, "0x%02x ", itrm->in.queue.data[i]);

		fprintf(stderr, "\n");
		fflush(stderr);
	}
#endif /* DEBUG_ITRM_QUEUE */

	/* el == -1 means itrm->in.queue appears to be the beginning of an
	 *          escape sequence but it is not yet complete.  Set a timer;
	 *          if it times out, then assume it wasn't an escape sequence
	 *          after all.
	 * el == 0 means this function has not yet figured out what the data
	 *         in itrm->in.queue is, but some possibilities remain.
	 *         One of them will be chosen before returning.
	 * el > 0 means some bytes were successfully parsed from the beginning
	 *        of itrm->in.queue and should now be removed from there.
	 *        However, this does not always imply an event will be queued.
	 */

	/* ELinks should also recognize U+009B CONTROL SEQUENCE INTRODUCER
	 * as meaning the same as ESC 0x5B, and U+008F SINGLE SHIFT THREE as
	 * meaning the same as ESC 0x4F, but those cannot yet be implemented
	 * because of bug 777: the UTF-8 decoder is run too late.  */
	if (itrm->in.queue.data[0] == ASCII_ESC) {
		if (itrm->in.queue.len < 2) {
			el = -1;
		} else if (itrm->in.queue.data[1] == 0x5B /* CSI */) {
			el = decode_terminal_escape_sequence(itrm, &ev);
		} else if (itrm->in.queue.data[1] == 0x4F /* SS3 */) {
			el = decode_terminal_application_key(itrm, &ev);
		} else if (itrm->in.queue.data[1] == ASCII_ESC) {
			/* ESC ESC can be either Alt-Esc or the
			 * beginning of e.g. ESC ESC 0x5B 0x41,
			 * which we should parse as Esc Up.  */
			if (itrm->in.queue.len < 3) {
				/* Need more data to figure it out.  */
				el = -1;
			} else if (itrm->in.queue.data[2] == 0x5B
				   || itrm->in.queue.data[2] == 0x4F) {
				/* The first ESC appears to be followed
				 * by an escape sequence.  Treat it as
				 * a standalone Esc.  */
				el = 1;
				set_kbd_event(itrm, &ev,
					      itrm->in.queue.data[0],
					      KBD_MOD_NONE);
			} else {
				/* The second ESC of ESC ESC is not the
				 * beginning of any known escape sequence.
				 * This must be Alt-Esc, then.  */
				el = 2;
				set_kbd_event(itrm, &ev,
					      itrm->in.queue.data[1],
					      KBD_MOD_ALT);
			}
		}
		if (el == 0) {	/* Begins with ESC, but none of the above */
			el = 2;
			set_kbd_event(itrm, &ev, itrm->in.queue.data[1],
				      KBD_MOD_ALT);
		}

	} else if (itrm->in.queue.data[0] == 0) {
		static const struct term_event_keyboard os2xtd[256] = {
#include "terminal/key.inc"
		};

		if (itrm->in.queue.len < 2)
			el = -1;
		else {
			el = 2;
			set_kbd_interlink_event(&ev,
						os2xtd[itrm->in.queue.data[1]].key,
						os2xtd[itrm->in.queue.data[1]].modifier);
		}
	}

	if (el == 0) {
		el = 1;
		set_kbd_event(itrm, &ev, itrm->in.queue.data[0], KBD_MOD_NONE);
	}

	/* The call to decode_terminal_escape_sequence() might have changed the
	 * keyboard event to a mouse event. */
	if (ev.ev == EVENT_MOUSE || ev.info.keyboard.key != KBD_UNDEF)
		itrm_queue_event(itrm, (char *) &ev, sizeof(ev));

return_without_event:
	if (el == -1) {
		install_timer(&itrm->timer, ESC_TIMEOUT, (void (*)(void *)) kbd_timeout,
			      itrm);
		return 0;
	} else {
		assertm(itrm->in.queue.len >= el, "event queue underflow");
		if_assert_failed { itrm->in.queue.len = el; }

		itrm->in.queue.len -= el;
		if (itrm->in.queue.len)
			memmove(itrm->in.queue.data, itrm->in.queue.data + el, itrm->in.queue.len);

		if (itrm->in.queue.len < ITRM_IN_QUEUE_SIZE)
			handle_itrm_stdin(itrm);

		return el;
	}
}


/** A select_handler_T read_func for itrm_in.std.  This is called when
 * characters typed by the user arrive from the terminal. */
static void
in_kbd(struct itrm *itrm)
{
	int r;

	if (!can_read(itrm->in.std)) return;

	kill_timer(&itrm->timer);

	if (itrm->in.queue.len >= ITRM_IN_QUEUE_SIZE) {
		unhandle_itrm_stdin(itrm);
		while (process_queue(itrm));
		return;
	}

	r = safe_read(itrm->in.std, itrm->in.queue.data + itrm->in.queue.len,
		      ITRM_IN_QUEUE_SIZE - itrm->in.queue.len);
	if (r <= 0) {
		free_itrm(itrm);
		return;
	}

	itrm->in.queue.len += r;
	if (itrm->in.queue.len > ITRM_IN_QUEUE_SIZE) {
		ERROR(gettext("Too many bytes read from the itrm!"));
		itrm->in.queue.len = ITRM_IN_QUEUE_SIZE;
	}

	while (process_queue(itrm));
}

/** Enable reading from itrm_in.std.  ELinks will read any available
 * bytes from the tty into itrm->in.queue and then parse them.
 * Reading should be enabled whenever itrm->in.queue is not full and
 * itrm->blocked is 0.  */
static void
handle_itrm_stdin(struct itrm *itrm)
{
	assert(itrm->in.std >= 0);
	if_assert_failed return;

	set_handlers(itrm->in.std, (select_handler_T) in_kbd, NULL,
		     (select_handler_T) free_itrm, itrm);
}

/** Disable reading from itrm_in.std.  Reading should be disabled
 * whenever itrm->in.queue is full (there is no room for the data)
 * or itrm->blocked is 1 (other processes may read the data).  */
static void
unhandle_itrm_stdin(struct itrm *itrm)
{
	assert(itrm->in.std >= 0);
	if_assert_failed return;

	set_handlers(itrm->in.std, (select_handler_T) NULL, NULL,
		     (select_handler_T) free_itrm, itrm);
}
