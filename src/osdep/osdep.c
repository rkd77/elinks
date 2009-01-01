/* Features which vary with the OS */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_IO_H
#include <io.h> /* For win32 && set_bin(). */
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>	/* Need to be after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

/* This is for some exotic TOS mangling when handling passive FTP sockets. */
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#else
#ifdef HAVE_NETINET_IN_SYSTEM_H
#include <netinet/in_system.h>
#endif
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LOCALE_H
/* For the sake of SunOS, keep this away from files including
 * intl/gettext/libintl.h because <locale.h> includes system <libintl.h> which
 * either includes system gettext header or contains gettext function
 * declarations. */
#include <locale.h>
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif


#include "elinks.h"

#include "main/select.h"
#include "osdep/osdep.h"
#include "osdep/signals.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


/* Set a file descriptor to non-blocking mode. It returns a non-zero value
 * on error. */
int
set_nonblocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags | O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 1;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

/* Set a file descriptor to blocking mode. It returns a non-zero value on
 * error. */
int
set_blocking_fd(int fd)
{
#if defined(O_NONBLOCK) || defined(O_NDELAY)
	int flags = fcntl(fd, F_GETFL, 0);

	if (flags < 0) return -1;
#if defined(O_NONBLOCK)
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#else
	return fcntl(fd, F_SETFL, flags & ~O_NDELAY);
#endif

#elif defined(FIONBIO)
	int flag = 0;

	return ioctl(fd, FIONBIO, &flag);
#else
	return 0;
#endif
}

void
set_ip_tos_throughput(int socket)
{
#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
	int on = IPTOS_THROUGHPUT;

	setsockopt(socket, IPPROTO_IP, IP_TOS, (char *) &on, sizeof(on));
#endif
}

int
get_e(unsigned char *env)
{
	char *v = getenv(env);

	return (v ? atoi(v) : 0);
}

unsigned char *
get_cwd(void)
{
	int bufsize = 128;
	unsigned char *buf;

	while (1) {
		buf = mem_alloc(bufsize);
		if (!buf) return NULL;
		if (getcwd(buf, bufsize)) return buf;
		mem_free(buf);

		if (errno == EINTR) continue;
		if (errno != ERANGE) return NULL;
		bufsize += 128;
	}

	return NULL;
}

void
set_cwd(unsigned char *path)
{
	if (path) while (chdir(path) && errno == EINTR);
}

unsigned char *
get_shell(void)
{
	unsigned char *shell = GETSHELL;

	if (!shell || !*shell)
		shell = DEFAULT_SHELL;

	return shell;
}


/* Terminal size */

#if !defined(CONFIG_OS_OS2) && !defined(CONFIG_OS_WIN32)

static void
sigwinch(void *s)
{
	((void (*)(void)) s)();
}

void
handle_terminal_resize(int fd, void (*fn)(void))
{
	install_signal_handler(SIGWINCH, sigwinch, fn, 0);
}

void
unhandle_terminal_resize(int fd)
{
	install_signal_handler(SIGWINCH, NULL, NULL, 0);
}

void
get_terminal_size(int fd, int *x, int *y)
{
	struct winsize ws;

	if (ioctl(1, TIOCGWINSZ, &ws) != -1) {
		*x = ws.ws_col;
		*y = ws.ws_row;
	} else {
		*x = 0;
		*y = 0;
	}

	if (!*x) {
		*x = get_e("COLUMNS");
		if (!*x) *x = DEFAULT_TERMINAL_WIDTH;
	}
	if (!*y) {
		*y = get_e("LINES");
		if (!*y) *y = DEFAULT_TERMINAL_HEIGHT;
	}
}

#endif

/* Pipe */

#if defined(CONFIG_OS_UNIX) || defined(CONFIG_OS_BEOS) || defined(CONFIG_OS_RISCOS)

void
set_bin(int fd)
{
}

int
c_pipe(int *fd)
{
	return pipe(fd);
}

#elif defined(CONFIG_OS_OS2) || defined(CONFIG_OS_WIN32)

void
set_bin(int fd)
{
	setmode(fd, O_BINARY);
}

int
c_pipe(int *fd)
{
	int r = pipe(fd);

	if (!r) {
		set_bin(fd[0]);
		set_bin(fd[1]);
	}

	return r;
}

#endif

/* Exec */

int
is_twterm(void) /* Check if it make sense to call a twterm. */
{
	static int tw = -1;

	if (tw == -1) tw = !!getenv("TWDISPLAY");

	return tw;
}

int
is_gnuscreen(void)
{
	static int screen = -1;

	if (screen == -1) screen = !!getenv("STY");

	return screen;
}


#if defined(CONFIG_OS_UNIX) || defined(CONFIG_OS_WIN32)

int
is_xterm(void)
{
	static int xt = -1;

	if (xt == -1) {
		/* Paraphrased from debian bug 228977:
		 *
		 * It is not enough to simply check the DISPLAY env variable,
		 * as it is pretty legal to have a DISPLAY set. While these
		 * days this practice is pretty uncommon, it still makes sense
		 * sometimes, especially for people who prefer the text mode
		 * for some reason. Only relying on DISPLAY will results in bad
		 * codes being written to the terminal.
		 *
		 * Any real xterm derivative sets WINDOWID as well.
		 * Unfortunately, konsole is an exception, and it needs to be
		 * checked for separately.
		 *
		 * FIXME: The code below still fails to detect some terminals
		 * that do support a title (like the popular PuTTY ssh client).
		 * In general, proper xterm detection is a nightmarish task...
		 *
		 * -- Adam Borowski <kilobyte@mimuw.edu.pl> */
		unsigned char *display = getenv("DISPLAY");
		unsigned char *windowid = getenv("WINDOWID");

		if (!windowid || !*windowid)
			windowid = getenv("KONSOLE_DCOP_SESSION");
		xt = (display && *display && windowid && *windowid);
	}

	return xt;
}

#endif

unsigned int resize_count = 0;

#ifndef CONFIG_OS_OS2

#if !(defined(CONFIG_OS_BEOS) && defined(HAVE_SETPGID)) && !defined(CONFIG_OS_WIN32)

int
exe(unsigned char *path)
{
	return system(path);
}

#endif

static unsigned char *clipboard;

unsigned char *
get_clipboard_text(void)
{
	/* The following support for GNU Screen's clipboard is
	 * disabled for two reasons:
	 *
	 * 1. It does not actually return the string from that
	 *    clipboard, but rather causes the clipboard contents to
	 *    appear in stdin.	get_clipboard_text is normally called
	 *    because the user pressed a Paste key in an input field,
	 *    so the characters end up being inserted in that field;
	 *    but if there are newlines in the clipboard, then the
	 *    field may lose focus, in which case the remaining
	 *    characters may trigger arbitrary actions in ELinks.
	 *
	 * 2. It pastes from both GNU Screen's clipboard and the ELinks
	 *    internal clipboard.  Because set_clipboard_text also sets
	 *    them both, the same text would typically get pasted twice.
	 *
	 * Users can instead use the GNU Screen key bindings to run the
	 * paste command.  This method still suffers from problem 1 but
	 * any user of GNU Screen should know that already.  */
#if 0
	/* GNU Screen's clipboard */
	if (is_gnuscreen()) {
		struct string str;

		if (!init_string(&str)) return NULL;

		add_to_string(&str, "screen -X paste .");
		if (str.length) exe(str.source);
		if (str.source) done_string(&str);
	}
#endif

	return stracpy(empty_string_or_(clipboard));
}

void
set_clipboard_text(unsigned char *data)
{
	/* GNU Screen's clipboard */
	if (is_gnuscreen()) {
		struct string str;

		if (!init_string(&str)) return;

		add_to_string(&str, "screen -X register . ");
		add_shell_quoted_to_string(&str, data, strlen(data));

		if (str.length) exe(str.source);
		if (str.source) done_string(&str);
	}

	/* Shouldn't complain about leaks. */
	if (clipboard) free(clipboard);
	clipboard = strdup(data);
}

/* Set xterm-like term window's title. */
void
set_window_title(unsigned char *title, int codepage)
{
	struct string filtered;

#ifndef HAVE_SYS_CYGWIN_H
	/* Check if we're in a xterm-like terminal. */
	if (!is_xterm() && !is_gnuscreen()) return;
#endif

	if (!init_string(&filtered)) return;

	/* Copy title to filtered if different from NULL */
	if (title) {
		unsigned char *scan = title;
		unsigned char *end = title + strlen(title);

		/* Remove control characters, so that they cannot
		 * interfere with the command we send to the terminal.
		 * However, do not attempt to limit the title length
		 * to terminal width, because the title is usually
		 * drawn in a different font anyway.  */
		/* Note that this is the right place where to do it, since
		 * potential alternative set_window_title() routines might
		 * want to take different precautions. */
		for (;;) {
			unsigned char *charbegin = scan;
			unicode_val_T unicode
				= cp_to_unicode(codepage, &scan, end);
			int charlen = scan - charbegin;

			if (unicode == UCS_NO_CHAR)
				break;

			/* This need not recognize all Unicode control
			 * characters.  Only those that can make the
			 * terminal misparse the command.  */
			if (unicode < 0x20
			    || (unicode >= 0x7F && unicode < 0xA0))
				continue;

			/* If the title is getting too long, truncate
			 * it and add an ellipsis.
			 *
			 * xterm entirely rejects 1024-byte or longer
			 * titles.  GNU Screen 4.00.03 misparses
			 * titles longer than 765 bytes, and is unable
			 * to display the title in hardstatus if the
			 * title and other stuff together exceed 766
			 * bytes.  So set the limit quite a bit lower.  */
			if (filtered.length + charlen >= 600 - 3) {
				add_to_string(&filtered, "...");
				break;
			}

			add_bytes_to_string(&filtered, charbegin, charlen);
		}
	}

	/* Send terminal escape sequence + title string */
	printf("\033]0;%s\a", filtered.source);

#if 0
	/* Miciah don't like this so it is disabled because it changes the
	 * default window name. --jonas */
	/* Set the GNU screen window name */
	if (is_gnuscreen())
		printf("\033k%s\033\134", filtered.source);
#endif

	fflush(stdout);

	done_string(&filtered);
}

#ifdef HAVE_X11
static int x_error = 0;

static int
catch_x_error(void)
{
	x_error = 1;
	return 0;
}
#endif

unsigned char *
get_window_title(void)
{
#ifdef HAVE_X11
	/* Following code is stolen from our beloved vim. */
	unsigned char *winid;
	Display *display;
	Window window, root, parent, *children;
	XTextProperty text_prop;
	Status status;
	unsigned int num_children;
	unsigned char *ret = NULL;

	if (!is_xterm())
		return NULL;

	winid = getenv("WINDOWID");
	if (!winid)
		return NULL;
	window = (Window) atol(winid);
	if (!window)
		return NULL;

	display = XOpenDisplay(NULL);
	if (!display)
		return NULL;

	/* If WINDOWID is bad, we don't want X to abort us. */
	x_error = 0;
	XSetErrorHandler((int (*)(Display *, XErrorEvent *)) catch_x_error);

	status = XGetWMName(display, window, &text_prop);
	/* status = XGetWMIconName(x11_display, x11_window, &text_prop); */
	while (!x_error && (!status || !text_prop.value)) {
		if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
			break;
		if (children)
			XFree((void *) children);
		if (parent == root || parent == 0)
			break;
		window = parent;
		status = XGetWMName(display, window, &text_prop);
	}

	if (!x_error && status && text_prop.value) {
		ret = stracpy(text_prop.value);
		XFree(text_prop.value);
	}

	XCloseDisplay(display);

	return ret;
#else
	/* At least reset the window title to a blank one. */
	return stracpy("");
#endif
}

int
resize_window(int width, int height, int old_width, int old_height)
{
#ifdef HAVE_X11
	/* Following code is stolen from our beloved vim. */
	unsigned char *winid;
	Display *display;
	Window window;
	Status status;
	XWindowAttributes attributes;

	if (!is_xterm())
		return -1;

	winid = getenv("WINDOWID");
	if (!winid)
		return -1;
	window = (Window) atol(winid);
	if (!window)
		return -1;

	display = XOpenDisplay(NULL);
	if (!display)
		return -1;

	/* If WINDOWID is bad, we don't want X to abort us. */
	x_error = 0;
	XSetErrorHandler((int (*)(Display *, XErrorEvent *)) catch_x_error);

	status = XGetWindowAttributes(display, window, &attributes);

	while (!x_error && !status) {
		Window root, parent, *children;
		unsigned int num_children;

		if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
			break;
		if (children)
			XFree((void *) children);
		if (parent == root || parent == 0)
			break;
		window = parent;
		status = XGetWindowAttributes(display, window, &attributes);
	}

	if (!x_error && status) {
		XSizeHints *size_hints;
		long mask;
		int px_width = 0;
		int px_height = 0;

		/* With xterm 210, a window with 80x24 characters at
		 * a 6x13 font appears to have 484x316 pixels; both
		 * the width and height include four extra pixels.
		 * Computing a new size by scaling these values often
		 * results in windows that cannot display as many
		 * characters as was intended.  We can do better if we
		 * can find out the actual size of character cells.
		 * If the terminal emulator has set a window size
		 * increment, assume that is the cell size.  */
		size_hints = XAllocSizeHints();
		if (size_hints != NULL
		    && XGetWMNormalHints(display, window, size_hints, &mask)
		    && (mask & PResizeInc) != 0) {
			px_width = attributes.width
				+ (width - old_width) * size_hints->width_inc;
			px_height = attributes.height
				+ (height - old_height) * size_hints->height_inc;
		}
		if (px_width <= 0 || px_height <= 0) {
			double ratio_width = (double) attributes.width  / old_width;
			double ratio_height = (double) attributes.height / old_height;

			px_width  = (int) ((double) width * ratio_width);
			px_height = (int) ((double) height * ratio_height);
		}

		if (size_hints)
			XFree(size_hints);

		status = XResizeWindow(display, window, px_width, px_height);
		while (!x_error && !status) {
			Window root, parent, *children;
			unsigned int num_children;

			if (!XQueryTree(display, window, &root, &parent, &children, &num_children))
				break;
			if (children)
				XFree((void *) children);
			if (parent == root || parent == 0)
				break;
			window = parent;
			status = XResizeWindow(display, window, px_width, px_height);
		}
	}

	XCloseDisplay(display);

	return 0;
#else
	return -1;
#endif
}

#endif


/* Threads */

#if defined(HAVE_BEGINTHREAD) || defined(CONFIG_OS_BEOS)

struct tdata {
	void (*fn)(void *, int);
	int h;
	unsigned char data[1];
};

void
bgt(struct tdata *t)
{
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
	t->fn(t->data, t->h);
	write(t->h, "x", 1);
	close(t->h);
	free(t);
}

#else

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	pid_t pid;

	if (c_pipe(p) < 0) return -1;
	if (set_nonblocking_fd(p[0]) < 0) return -1;
	if (set_nonblocking_fd(p[1]) < 0) return -1;

	pid = fork();
	if (!pid) {
		struct terminal *term;

		/* Close input in this thread; otherwise, if it will live
		 * longer than its parent, it'll block the terminal until it'll
		 * quit as well; this way it will hopefully just die unseen and
		 * in background, causing no trouble. */
		/* Particularly, when async dns resolving was in progress and
		 * someone quitted ELinks, it could make a delay before the
		 * terminal would be really freed and returned to shell. */
		foreach (term, terminals)
			if (term->fdin > 0)
				close(term->fdin);

		close(p[0]);
		fn(ptr, p[1]);
		write(p[1], "x", 1);
		close(p[1]);
		/* We use _exit() here instead of exit(), see
		 * http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC6 for
		 * reasons. Fixed by Sven Neumann <sven@convergence.de>. */
		_exit(0);
	}
	if (pid == -1) {
		close(p[0]);
		close(p[1]);
		return -1;
	}

	close(p[1]);
	return p[0];
}

#endif


#ifndef OS2_MOUSE
void
want_draw(void)
{
}

void
done_draw(void)
{
}
#endif


#if !defined(CONFIG_OS_WIN32)
int
get_output_handle(void)
{
	return 1;
}

int
get_ctl_handle(void)
{
	static int fd = -1;

	if (isatty(0)) return 0;
	if (fd < 0) fd = open("/dev/tty", O_RDONLY);
	return fd;
}
#endif


#if !defined(CONFIG_OS_BEOS) && !(defined(HAVE_BEGINTHREAD) && defined(HAVE_READ_KBD)) \
	&& !defined(CONFIG_OS_WIN32)

int
get_input_handle(void)
{
	return get_ctl_handle();
}

#endif

#ifndef CONFIG_OS_WIN32

void
init_osdep(void)
{
#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif
}

#endif

#if defined(CONFIG_OS_UNIX) || defined(CONFIG_OS_OS2) || defined(CONFIG_OS_RISCOS)

void
terminate_osdep(void)
{
}

#endif

#ifndef CONFIG_OS_BEOS

void
block_stdin(void)
{
}

void
unblock_stdin(void)
{
}

#endif


void
elinks_cfmakeraw(struct termios *t)
{
	/* This elinks_cfmakeraw() intentionally leaves the following
	 * settings unchanged, even though the standard cfmakeraw()
	 * would change some of them:
	 *
	 * - c_cflag & CSIZE: number of bits per character.
	 *   Bug 54 asked ELinks not to change this.
	 * - c_cflag & (PARENB | PARODD): parity bit in characters.
	 *   Bug 54 asked ELinks not to change this.
	 * - c_iflag & (IXON | IXOFF | IXANY): XON/XOFF flow control.
	 *
	 * The reasoning is, if the user has set up unusual values for
	 * those settings before starting ELinks, then the terminal
	 * probably expects those values and ELinks should not mess
	 * with them.  */
	t->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cc[VMIN] = 1;
	t->c_cc[VTIME] = 0;
}

#if !defined(CONFIG_MOUSE) || (!defined(CONFIG_GPM) && !defined(CONFIG_SYSMOUSE) && !defined(OS2_MOUSE))

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	return NULL;
}

void
unhandle_mouse(void *data)
{
}

void
suspend_mouse(void *data)
{
}

void
resume_mouse(void *data)
{
}

#endif

#ifndef CONFIG_OS_WIN32
/* Create a bitmask consisting from system-independent envirnoment modifiers.
 * This is then complemented by system-specific modifiers in an appropriate
 * get_system_env() routine. */
static int
get_common_env(void)
{
	int env = 0;

	if (is_xterm()) env |= ENV_XWIN;
	if (is_twterm()) env |= ENV_TWIN;
	if (is_gnuscreen()) env |= ENV_SCREEN;

	/* ENV_CONSOLE is always set now and indicates that we are working w/ a
	 * displaying-capable character-adressed terminal. Sounds purely
	 * theoretically now, but it already makes some things easier and it
	 * could give us interesting opportunities later (after graphical
	 * frontends will be introduced, when working in some mysterious daemon
	 * mode or who knows what ;). --pasky */
	env |= ENV_CONSOLE;

	return env;
}
#endif

#if defined(CONFIG_OS_UNIX) || defined(CONFIG_OS_RISCOS)
int
get_system_env(void)
{
	return get_common_env();
}
#endif


int
can_resize_window(int environment)
{
	return !!(environment & (ENV_OS2VIO | ENV_XWIN));
}

#ifndef CONFIG_OS_OS2
int
can_open_os_shell(int environment)
{
	return 1;
}

void
set_highpri(void)
{
}
#endif


unsigned char *
get_system_str(int xwin)
{
	return xwin ? SYSTEM_STR "-xwin" : SYSTEM_STR;
}
